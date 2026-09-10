# -*- coding: utf-8 -*-
"""从 CDDA 的 cataclysm-dda.mo 批量提取枪械相关中文译名。

注意：Python 的 gettext 会把部分条目存成元组键 (msgid, 0)，
所以不能只用 dict.get(msgid)，必须同时尝试元组形式。
"""
import gettext

MO = r"C:\Users\34068\Project\Cataclysm\CDDA\lang\mo\zh_CN\LC_MESSAGES\cataclysm-dda.mo"
with open(MO, "rb") as f:
    T = gettext.GNUTranslations(f)
C = T._catalog


def zh(s):
    """查译文；找不到返回 None。"""
    for key in (s, (s, 0)):
        v = C.get(key)
        if isinstance(v, str) and v:
            return v
    return None


def table(title, keys):
    print("=" * 74)
    print(title)
    print("=" * 74)
    miss = 0
    for k in keys:
        r = zh(k)
        if r is None:
            miss += 1
            print("  %-44s | %s" % (k, "—"))
        else:
            print("  %-44s | %s" % (k, r))
    print("  （%d 项中 %d 项无译文）" % (len(keys), miss))
    print()


def fuzzy(title, needle, maxlen=60):
    print("=" * 74)
    print(title)
    print("=" * 74)
    hit = 0
    for k, v in C.items():
        if not isinstance(k, str) or not isinstance(v, str) or not v:
            continue
        if needle.lower() in k.lower() and len(k) <= maxlen:
            print("  %-44s | %s" % (k, v))
            hit += 1
    if not hit:
        print("  (无匹配)")
    print()


table("一、配件名（gunmod）", [
    "bipod", "forward grip", "offset grip", "high end hand guard",
    "recoil stock", "adjustable stock", "folding stock", "cheek pad",
    "muzzle brake", "compensator", "ported barrel", "shortened barrel",
    "red dot sight", "holographic sight", "rifle scope", "sight magnifier",
    "underbarrel laser sight", "rail laser sight",
    "brass catcher", "bayonet lug",
])

table("二、枪械与上机匣", [
    "M4 carbine", "M4A1 carbine", "Mk 18 CQBR carbine",
    "Glock pistol", "Glock 20 pistol", "Glock 29 pistol",
    "M16A4 rifle", "M16 burst rifle",
    "mid-length .223 upper receiver", "carbine .223 upper receiver",
    "rifle .223 upper receiver", "CQB .223 upper receiver",
    "5.56x45mm / .223", "10mm Auto",
])

table("三、改装槽位", [
    "muzzle", "barrel", "bore", "mechanism", "rail", "sights",
    "stock", "stock accessory", "underbarrel", "sling", "brass catcher",
])

table("四、属性界面标签", [
    "Dispersion: ", "Sight dispersion: ", "Sight dispersion (point shooting): ",
    "Effective recoil: ", "Theoretical minimum recoil: ",
    "Handling modifier: ", "Range modifier: ", "Dispersion modifier: ",
    "Minimum strength required modifier: ", "Reload modifier: ",
    "Ranged damage", "Armor-pierce: ", "Critical multiplier: ",
    "Maximum range: ", "Even chance of good hit at range: ",
    "Time to reach aim level: ", "Loudness with current fire mode: ",
    "Reload time: ", "Ammunition consumed per shot: ",
    "Aim speed: ", "Aim speed modifier: ",
])

fuzzy("五、模糊查找：手电/激光模块", "flashlight-laser")
fuzzy("六、模糊查找：弹匣变体后缀", "round magazine")
