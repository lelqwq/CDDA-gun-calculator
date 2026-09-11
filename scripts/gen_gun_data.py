#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_gun_data.py — 从 Cataclysm-DDA 的游戏数据生成 gunlab 的 C++ 数据文件

用法：
    python scripts/gen_gun_data.py --game "C:\\path\\to\\CDDA"
    python scripts/gen_gun_data.py --game "<路径>" --mods Aftershock,Xedra_Evolved
    python scripts/gen_gun_data.py --game "<路径>" --out src/generated

做什么：
    1. 读 data/json 下的全部 JSON（可按 --mods 追加 mod）
    2. 解析 copy-from 继承链（含 proportional / relative，递归到嵌套对象）
    3. 从游戏的 .mo 翻译文件里查每个条目的官方中文名
    4. 生成 src/generated/ 下的三个 C++ 文件

注意：
    · extend / delete 被有意跳过 —— 它们只操作 effects / flags 这类数组，
      gunlab 不使用这些字段。若要新增依赖它们的列，需在此补上。
    · 变体（variants）作为别名并入父条目，不单独成条。变体的重量微差被丢弃。
    · 变体的别名取自 "variants" 数组的 name 字段。
"""

import argparse
import glob
import io
import json
import os
import re
import sys

# =============================================================================
#  单位解析
# =============================================================================

WEIGHT_UNITS = {"g": 1.0, "mg": 0.001, "kg": 1000.0}
VOLUME_UNITS = {"ml": 1.0, "L": 1000.0, "cl": 10.0, "dl": 100.0}
LENGTH_UNITS = {"mm": 1.0, "cm": 10.0, "m": 1000.0}

_NUM_RE = re.compile(r"^\s*(-?[\d.]+)\s*([A-Za-z]*)\s*$")


def parse_unit(value, units, default=0.0):
    """把 "880 g" / "1.5 L" / "470 mm" 解析成数值。无法解析时返回 default。"""
    if value is None:
        return default
    if isinstance(value, (int, float)):
        return float(value)
    m = _NUM_RE.match(str(value))
    if not m:
        return default
    amount, unit = float(m.group(1)), m.group(2)
    if unit == "":
        return amount
    if unit not in units:
        return default
    return amount * units[unit]


# =============================================================================
#  数据加载与继承解析
# =============================================================================

# ---- 哪些「枪」其实不是枪械 ------------------------------------------------
# 游戏数据里有几类条目带 GUN 子类型，但不是玩家能装备的枪械。判据用游戏
# 自己的标志和目录位置，不硬编码条目 id —— 游戏加新条目时不用改这里。

# 这些目录里的东西按定义就不是「能拿到的枪」：
#   obsoletion_and_migration_*  已从游戏里删除、只为老存档留着的定义
#                               （如 PPSh-41、Saiga-410、American-180）
#   monster_special_attacks     怪物特殊攻击的数值模板（acid dart gun 之类）
EXCLUDE_DIRS = ("obsoletion_and_migration", "monster_special_attacks")

# 这些标志一出现就说明不是普通枪械：
#   PRIMITIVE_RANGED_WEAPON  弓弩、投石索、掷矛器。★ 光按技能滤不掉 ——
#                            有些弩在游戏里归在 rifle/pistol 技能下
#                            （crossbow / hand_crossbow / bullet_crossbow…）
#   BIONIC_WEAPON            义体武器，玩家装备不了
#   PSEUDO                   只作数值模板、游戏里拿不到的伪物品
EXCLUDE_FLAGS = ("PRIMITIVE_RANGED_WEAPON", "BIONIC_WEAPON", "PSEUDO")


def exclude_reason(db, oid, o):
    """返回排除原因（字符串）；None 表示保留。"""
    for p in db.paths_of.get(oid, []):
        norm = p.replace("\\", "/")
        for d in EXCLUDE_DIRS:
            if d in norm:
                return d

    r = db.resolve(o)
    flags = r.get("flags") or []
    for f in EXCLUDE_FLAGS:
        if f in flags:
            return f

    # 没有任何「能开火」的模式 —— 典型例子是「可拆卸反曲弓（折叠）」，
    # 它的模式是 [ "DEFAULT", "disassembled", 0, [ "MELEE" ] ]：发数 0，
    # 是收纳状态、只能近战，要用 use_action 组装回展开状态才能射。
    # 这种没有 PRIMITIVE_RANGED_WEAPON（那一版才挂），得靠发数滤。
    modes = r.get("modes")
    if modes:
        shootable = any(isinstance(m, list) and len(m) >= 3
                        and isinstance(m[2], int) and m[2] >= 1
                        for m in modes)
        if not shootable:
            return "无可射击模式"
    return None


class Database:
    def __init__(self):
        self.by_id = {}        # id -> obj
        self.by_abstract = {}  # abstract -> obj
        self.src_of = {}       # id -> 来源（"core" 或 mod 名）
        self.paths_of = {}     # id -> 定义所在文件的列表（判断要不要排除时用）
        self.skipped = 0

    @staticmethod
    def _is_item(o):
        """判断是否是一个真正的物品条目。

        必须过滤掉同 id 的"同名不同类"条目 —— 例如
        data/json/ascii_art/ammo/10mm.json 里有一条 {"type":"ascii_art",
        "id":"10mm_fmj"}，它按字母序排在 items/ 之前，会把真条目挤掉，
        导致解析出的弹药数值全为 0。
        """
        if o.get("subtypes"):
            return True
        return o.get("type") == "ITEM"

    def load_dir(self, root, source):
        """加载 root 下所有 .json。source 用于标记数据来源。"""
        for fp in glob.glob(os.path.join(root, "**", "*.json"), recursive=True):
            try:
                data = json.load(io.open(fp, encoding="utf-8"))
            except Exception:
                self.skipped += 1          # 地图生成文件等，与枪械无关
                continue
            for o in (data if isinstance(data, list) else [data]):
                if not isinstance(o, dict):
                    continue
                if o.get("abstract"):
                    a = o["abstract"]
                    self.by_abstract[a if isinstance(a, str) else a[0]] = o
                    continue
                if not self._is_item(o):
                    continue               # 字符画 / 物品组 / 怪物等同名条目
                ids = o.get("id")
                if not ids:
                    continue
                for one in (ids if isinstance(ids, list) else [ids]):
                    if not isinstance(one, str):
                        continue
                    if one not in self.by_id:
                        self.by_id[one] = o
                        self.src_of[one] = source
                    self.paths_of.setdefault(one, []).append(fp)

    # ---- 继承解析 ----------------------------------------------------------

    def _apply_ops(self, out, ops, multiply):
        """就地应用 proportional（乘）/ relative（加）。递归进嵌套对象。"""
        for field, v in ops.items():
            if field not in out:
                continue
            cur = out[field]
            if isinstance(cur, dict) and isinstance(v, dict):
                new = dict(cur)
                self._apply_ops(new, v, multiply)
                out[field] = new
            elif isinstance(cur, (int, float)) and isinstance(v, (int, float)) \
                    and not isinstance(cur, bool):
                out[field] = cur * v if multiply else cur + v

    def _merge(self, base, child):
        """child 覆盖 base；先处理 proportional/relative，再覆盖普通字段。"""
        out = dict(base)
        if "proportional" in child:
            self._apply_ops(out, child["proportional"], True)
        if "relative" in child:
            self._apply_ops(out, child["relative"], False)
        for k, v in child.items():
            # extend / delete 有意跳过（只动数组，gunlab 不用）
            if k in ("copy-from", "abstract", "proportional", "relative",
                     "extend", "delete"):
                continue
            out[k] = v
        return out

    def resolve(self, obj, _depth=0):
        """递归解析 copy-from 继承链。"""
        if _depth > 16 or not isinstance(obj, dict):
            return obj
        parent = obj.get("copy-from")
        if not parent:
            return dict(obj)
        src = self.by_abstract.get(parent) or self.by_id.get(parent)
        if src is None:
            return dict(obj)          # 父条目缺失，退回自身
        return self._merge(self.resolve(src, _depth + 1), obj)

    def resolve_id(self, oid):
        o = self.by_id.get(oid)
        return self.resolve(o) if o else None


# =============================================================================
#  中文名提取（从游戏的 .mo 翻译文件）
# =============================================================================

class Translator:
    def __init__(self, mo_path):
        self.ok = False
        self.catalog = {}
        if mo_path and os.path.isfile(mo_path):
            try:
                import gettext
                with open(mo_path, "rb") as f:
                    self.catalog = gettext.GNUTranslations(f)._catalog
                self.ok = True
            except Exception as e:
                print("  [警告] 读取 .mo 失败：%s" % e, file=sys.stderr)

    def zh(self, s):
        """查中文译文；查不到返回 None。

        坑：Python 的 gettext 把部分条目存成元组键 (msgid, 0) 而非字符串，
        所以两种形式都要试。
        """
        if not s or not self.ok:
            return None
        for key in (s, (s, 0)):
            v = self.catalog.get(key)
            if isinstance(v, str) and v:
                return v
        return None

    def name_of(self, obj):
        """取条目的名字：优先 str，其次 str_sp。返回 (中文名, 英文名)。"""
        n = obj.get("name")
        if isinstance(n, dict):
            en = n.get("str") or n.get("str_sp") or ""
        elif isinstance(n, str):
            en = n
        else:
            en = ""
        return (self.zh(en) or en), en


# =============================================================================
#  工具
# =============================================================================

def subtypes_of(o):
    return [s for s in (o.get("subtypes") or []) if isinstance(s, str)]


def cstr(s):
    """生成 C++ 字符串字面量（UTF-8 源码，直接输出即可）。"""
    return '"' + str(s).replace("\\", "\\\\").replace('"', '\\"') + '"'


def cvec(items, indent=0):
    if not items:
        return "{}"
    return "{" + ", ".join(cstr(x) for x in items) + "}"


def fnum(x):
    """数值格式化：整数就不带小数点。"""
    if x is None:
        return "0"
    f = float(x)
    if f == int(f):
        return str(int(f))
    return ("%.4f" % f).rstrip("0").rstrip(".")


HEADER = """// =============================================================================
//  %s  —  自动生成，请勿手工编辑
// -----------------------------------------------------------------------------
//  由 scripts/gen_gun_data.py 从 Cataclysm-DDA 的游戏数据生成。
//  重新生成：
//      python scripts/gen_gun_data.py --game "<游戏目录>"
//
//  数据来源：%s
//  条目数：%d
//
//  ★ 字段顺序与 gun_data.h 里的结构体严格对应，改结构体必须同步改生成器 ★
// =============================================================================

#include "gun_data.h"

void %s()
{
    auto &V = %s;
    V.reserve(%d);
"""

FIELDS_NOTE = """
    // 字段顺序：%s
"""


# =============================================================================
#  主流程
# =============================================================================

def main():
    ap = argparse.ArgumentParser(description="生成 gunlab 的 C++ 数据文件")
    ap.add_argument("--game", default=r"C:\Users\34068\Project\Cataclysm\CDDA",
                    help="游戏根目录（含 data/ 和 lang/）")
    ap.add_argument("--mods", default="", help="额外包含的 mod，逗号分隔")
    ap.add_argument("--out", default="src/generated", help="输出目录")
    ap.add_argument("--lang", default="zh_CN", help="翻译语言")
    ap.add_argument("--no-translate", action="store_true", help="不查中文名")
    args = ap.parse_args()

    game = args.game
    json_root = os.path.join(game, "data", "json")
    mods_root = os.path.join(game, "data", "mods")
    mo_path = os.path.join(game, "lang", "mo", args.lang, "LC_MESSAGES", "cataclysm-dda.mo")

    if not os.path.isdir(json_root):
        print("错误：找不到 %s" % json_root, file=sys.stderr)
        return 1

    # 硬排除：TEST_DATA 是测试数据，Generic_Guns 是互斥的全面转换 mod
    ALWAYS_EXCLUDE = {"TEST_DATA", "Generic_Guns"}

    print("读取核心数据：%s" % json_root)
    db = Database()
    db.load_dir(json_root, "core")

    mods = [m.strip() for m in args.mods.split(",") if m.strip()]
    for m in mods:
        if m in ALWAYS_EXCLUDE:
            print("  [跳过] %s（已硬排除）" % m)
            continue
        p = os.path.join(mods_root, m)
        if not os.path.isdir(p):
            print("  [警告] mod 不存在：%s" % m, file=sys.stderr)
            continue
        print("读取 mod：%s" % m)
        db.load_dir(p, m)

    print("  索引实体 %d 个，抽象 %d 个，跳过 %d 个无法解析的文件"
          % (len(db.by_id), len(db.by_abstract), db.skipped))

    tr = Translator(None if args.no_translate else mo_path)
    print("中文翻译：%s" % ("已加载" if tr.ok else "未加载（将使用英文原名）"))

    # ---- 分类收集 ----------------------------------------------------------
    # 游戏数据里有几类条目虽然不是枪械却带着 GUN 子类型，先滤掉再统计。
    excluded = []          # (原因, id)
    guns, ammos, gunmods = [], [], []
    for oid, o in db.by_id.items():
        subs = subtypes_of(o)
        if o.get("abstract"):
            continue
        if "GUN" in subs:
            why = exclude_reason(db, oid, o)
            if why:
                excluded.append((why, oid))
                continue
            guns.append(oid)
        elif "AMMO" in subs:
            ammos.append(oid)
        elif "GUNMOD" in subs:
            gunmods.append(oid)

    print("  枪械 %d / 弹药 %d / 配件 %d" % (len(guns), len(ammos), len(gunmods)))
    if excluded:
        by_why = {}
        for why, oid in excluded:
            by_why.setdefault(why, []).append(oid)
        parts = ["%s %d" % (k, len(v)) for k, v in sorted(by_why.items(),
                                                          key=lambda x: -len(x[1]))]
        print("  排除非枪械条目 %d 条（%s）" % (len(excluded), "、".join(parts)))

    os.makedirs(args.out, exist_ok=True)

    # ---- 输出：枪械 --------------------------------------------------------
    n_alias = 0
    n_multi = 0           # 有连发模式（某模式发数 > 1）的枪
    lines = []
    no_disp = []          # 游戏数据里就没写 dispersion 的枪
    for oid in sorted(guns):
        o = db.by_id[oid]
        if o.get("variant_type") and o.get("variants") and o.get("abstract"):
            continue
        r = db.resolve(o)
        if r.get("dispersion") is None:
            no_disp.append(oid)
        name_zh, name_en = tr.name_of(r)
        aliases_zh, aliases_en = [], []

        # DDA 的变体机制：变体名往往才是这把枪在游戏里显示的名字，
        # 条目自身的 name 只是通用称呼（如 "Glock pistol" / "M16 auto rifle"）。
        # 取主名的优先级：
        #   1. 变体 id 等于条目 id   —— 如 glock_20 的变体 id="glock_20"
        #   2. 只有一个变体           —— 如 modular_m16_auto_rifle 只有 M16A3
        #   3. 否则用条目自身的 name  —— 如 modular_m4_carbine 有 M4A1 / Mk18 两个变体
        variants = [v for v in (r.get("variants") or []) if isinstance(v, dict)]
        primary_zh, primary_en = None, None
        for v in variants:
            if v.get("id") == oid:
                primary_zh, primary_en = tr.name_of(v)
                break
        if primary_en is None and len(variants) == 1:
            primary_zh, primary_en = tr.name_of(variants[0])
        if primary_en:
            # 条目自身的名字降级为别名
            if name_en and name_en != primary_en:
                aliases_en.append(name_en)
                aliases_zh.append(name_zh)
            name_zh, name_en = primary_zh, primary_en

        for v in variants:
            vz, ve = tr.name_of(v)
            if ve and ve != name_en:
                aliases_en.append(ve)
                aliases_zh.append(vz)
        n_alias += len(aliases_en)

        skill = r.get("skill") or ""
        gflags = [x for x in (r.get("flags") or []) if isinstance(x, str)]
        ammo_types = [a for a in (r.get("ammo") or []) if isinstance(a, str) and a.upper() != "NULL"]
        slots = []
        for loc in (r.get("valid_mod_locations") or []):
            if isinstance(loc, list) and loc and isinstance(loc[0], str):
                slots.append(loc[0])

        # 射击模式。JSON 是 [ [ 模式id, 显示名, 发数, 可选flag ], ... ]
        # （item_factory.cpp:3029 的注释）。第 4 个元素（如 "NPC_AVOID"）用不到。
        modes = []
        for m in (r.get("modes") or []):
            if (isinstance(m, list) and len(m) >= 3
                    and isinstance(m[0], str) and isinstance(m[2], int)):
                disp = m[1] if isinstance(m[1], str) else m[0]
                modes.append((m[0], disp, m[2]))

        # ★ 绝大多数枪在 JSON 里**不写 modes**（302/401），靠游戏加载器补：
        #   没有 DEFAULT 模式时插一个，发数恒为 1（0.I item_factory.cpp:705）。
        #   不补的话这些枪会一条模式都没有，后面取发数就取不到。
        if not any(m[0] == "DEFAULT" for m in modes):
            # 显示名按 item_factory.cpp:695 的 defmode_name()。
            # 它还有一条 `clip == 1 -> "manual"` 分支，但 clip 这个字段在全库
            # JSON 里一次都没出现（已迁到 pocket_data），所以那条走不到。
            defmode = ("revolver"
                       if skill == "pistol" and "RELOAD_ONE" in gflags
                       else "semi-auto")
            modes.insert(0, ("DEFAULT", defmode, 1))

        # 能不能卡壳：取决于该枪有没有可能产生 fault_gun_chamber_spent。
        # 转轮手枪、手动枪机、发射器这些没有这个故障（弹巢/枪机不靠子弹
        # 后坐力带动），所以永远不会「循环不到位」。
        # ★ faults 是数组字段，copy-from 时是**派生覆盖基类**而不是累加，
        #   resolve() 的 _merge 正好是这个语义，所以直接取解析后的值。
        faults = r.get("faults") or []
        can_jam = any(isinstance(x, dict) and x.get("fault") == "fault_gun_chamber_spent"
                      for x in faults)

        mode_list = ", ".join("{ %s, %s, %d }" % (cstr(m[0]), cstr(m[1]), m[2])
                              for m in modes)

        lines.append(
            '    add_gun(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, { %s }, %s, %s, %s);'
            % (
                cstr(oid), cstr(name_zh), cstr(name_en), cstr(skill),
                fnum(parse_unit(r.get("dispersion"), {}, 0)),
                # sight_dispersion 缺省是 30，不是 0  item_factory.cpp:3411
                fnum(parse_unit(r.get("sight_dispersion"), {}, 30)),
                fnum(parse_unit(r.get("handling"), {}, -1)),
                fnum(parse_unit(r.get("durability"), {}, 8)),
                fnum(parse_unit(r.get("recoil"), {}, 0)),
                fnum(parse_unit(r.get("weight"), WEIGHT_UNITS)),
                fnum(parse_unit(r.get("volume"), VOLUME_UNITS)),
                fnum(parse_unit(r.get("longest_side"), LENGTH_UNITS)),
                fnum(parse_unit(r.get("min_cycle_recoil"), {}, 0)),
                fnum(parse_unit(r.get("barrel_length"), LENGTH_UNITS)),
                "true" if "DISABLE_SIGHTS" in gflags else "false",
                cvec(ammo_types), cvec(slots), cvec(aliases_zh), cvec(aliases_en),
                mode_list,
                # 弓弩/投石索打完 recoil 直接回满，不参与连射累积
                "true" if "RELOAD_AND_SHOOT" in gflags else "false",
                "true" if can_jam else "false",
                cstr(db.src_of.get(oid, "core")),
            ))
        n_multi += 1 if any(m[2] > 1 for m in modes) else 0

    gun_field_note = ("id, name, name_en, skill, dispersion, sight_dispersion, handling, "
                      "durability, recoil, weight_g, volume_ml, longest_side_mm, "
                      "min_cycle_recoil, barrel_length_mm, disable_sights, ammo_types, mod_slots, "
                      "aliases_zh, aliases_en, modes, reload_and_shoot, can_jam, source")
    path = os.path.join(args.out, "gen_guns.cpp")
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER % ("gen_guns.cpp  生成的枪械数据", "core" if not mods else "core + " + ",".join(mods),
                          len(guns), "load_generated_guns", "g_guns", len(guns)))
        f.write(FIELDS_NOTE % gun_field_note)
        f.write("\n")
        f.write("\n".join(lines))
        f.write("\n}\n")
    print("  写出 %s（%d 条，含 %d 个别名，%d 把能连发）"
          % (path, len(guns), n_alias, n_multi))
    if no_disp:
        print("  [提示] %d 把枪在游戏数据里没有 dispersion 字段（按 C++ 默认值 0 处理）："
              % len(no_disp))
        print("         " + ", ".join(no_disp))

    # ---- 输出：弹药 --------------------------------------------------------
    lines = []
    for oid in sorted(ammos):
        r = db.resolve(db.by_id[oid])
        name_zh, name_en = tr.name_of(r)
        atype = r.get("ammo_type")
        if isinstance(atype, list):
            atype = atype[0] if atype else ""
        # 枪管长度 → 散布 的插值表（dispersion_considering_length 用）
        pairs = []
        for e in (r.get("dispersion_modifier") or []):
            if not isinstance(e, dict):
                continue
            bl = parse_unit(e.get("barrel_length"), LENGTH_UNITS, -1)
            dv = e.get("dispersion")
            if bl >= 0 and isinstance(dv, (int, float)):
                pairs.append("{%s, %s}" % (fnum(bl), fnum(dv)))
        disp_tbl = "{" + ", ".join(pairs) + "}"

        lines.append(
            '    add_ammo(%s, %s, %s, %s, %s, %s, %s, %s, %s);'
            % (
                cstr(oid), cstr(name_zh), cstr(name_en), cstr(atype or ""),
                fnum(parse_unit(r.get("recoil"), {}, 0)),
                fnum(parse_unit(r.get("dispersion"), {}, 0)),
                fnum(parse_unit(r.get("range"), {}, 0)),
                disp_tbl,
                cstr(db.src_of.get(oid, "core")),
            ))

    path = os.path.join(args.out, "gen_ammo.cpp")
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER % ("gen_ammo.cpp  生成的弹药数据", "core" if not mods else "core + " + ",".join(mods),
                          len(ammos), "load_generated_ammo", "g_ammo", len(ammos)))
        f.write(FIELDS_NOTE % ("id, name, name_en, ammo_type, recoil, dispersion, range, "
                               "disp_by_barrel, source"))
        f.write("\n")
        f.write("\n".join(lines))
        f.write("\n}\n")
    print("  写出 %s（%d 条）" % (path, len(ammos)))

    # ---- 输出：配件 --------------------------------------------------------
    lines = []
    for oid in sorted(gunmods):
        r = db.resolve(db.by_id[oid])
        name_zh, name_en = tr.name_of(r)
        loc = r.get("location") or ""
        ammo_mod = [a for a in (r.get("ammo_modifier") or []) if isinstance(a, str)]
        targets = [t for t in (r.get("mod_targets") or []) if isinstance(t, str)]
        flags = [x for x in (r.get("flags") or []) if isinstance(x, str)]
        # add_mod：装上后解锁的槽位，如 [[ "rail", 2 ], [ "sights", 1 ]]
        added = []
        for pair in (r.get("add_mod") or []):
            if isinstance(pair, list) and pair and isinstance(pair[0], str):
                added.append(pair[0])
        lines.append(
            '    add_gunmod(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s);'
            % (
                cstr(oid), cstr(name_zh), cstr(name_en), cstr(loc),
                fnum(parse_unit(r.get("handling_modifier"), {}, 0)),
                fnum(parse_unit(r.get("dispersion_modifier"), {}, 0)),
                fnum(parse_unit(r.get("aim_speed_modifier"), {}, 0)),
                fnum(parse_unit(r.get("sight_dispersion"), {}, -1)),
                fnum(parse_unit(r.get("field_of_view"), {}, -1)),
                fnum(parse_unit(r.get("weight"), WEIGHT_UNITS)),
                fnum(parse_unit(r.get("volume"), VOLUME_UNITS)),
                fnum(parse_unit(r.get("barrel_length"), LENGTH_UNITS)),
                "true" if "BIPOD" in flags else "false",
                "true" if "LASER_SIGHT" in flags else "false",
                "true" if "ZOOM" in flags else "false",
                cvec(ammo_mod), cvec(targets), cvec(added),
                cstr(db.src_of.get(oid, "core")),
            ))

    path = os.path.join(args.out, "gen_gunmods.cpp")
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER % ("gen_gunmods.cpp  生成的配件数据", "core" if not mods else "core + " + ",".join(mods),
                          len(gunmods), "load_generated_gunmods", "g_mods", len(gunmods)))
        f.write(FIELDS_NOTE % ("id, name, name_en, location, handling_modifier, "
                               "dispersion_modifier, aim_speed_modifier, sight_dispersion, "
                               "field_of_view, weight_g, volume_ml, barrel_length_mm, "
                               "bipod, laser_sight, zoom, "
                               "ammo_modifier, mod_targets, add_mod, source"))
        f.write("\n")
        f.write("\n".join(lines))
        f.write("\n}\n")
    print("  写出 %s（%d 条）" % (path, len(gunmods)))

    print("\n完成。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
