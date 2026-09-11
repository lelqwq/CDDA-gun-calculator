// =============================================================================
//  zh_cn.h  —  gunlab 的全部中文文本
// -----------------------------------------------------------------------------
//  这个文件里只有"给人看的字"。gunlab.cpp 里只有数学和逻辑。
//
//  译名来源：游戏本体 CDDA\lang\mo\zh_CN\LC_MESSAGES\cataclysm-dda.mo
//            （用 Python 的 gettext 提取，见 extract_zh.py）
//
//  想改中文？只动这一个文件，然后重新编译即可。
//  想加英文版？照抄一份 en_us.h，把 gunlab.cpp 顶部的 #include 换掉。
//
//  注意：GunMod.location（"underbarrel"）、Gun.skill（"rifle"）、GunMod.id
//  这些英文串是**逻辑用的键**（代码里有 has_mod("underbarrel") 之类的比较，
//  且与游戏 JSON 字段对应），所以不能改成中文 —— 但它们不会出现在输出里，
//  显示时一律经过下面的 skill() / slot() 转换。
// =============================================================================

#pragma once

#include <string>

namespace zh {

// =============================================================================
//  一、译名表（英文键 -> 中文）
// =============================================================================

// 技能名
//
//  中文取自游戏 skills.json 里各技能的 name 字段在 .mo 里的官方译文 ——
//  注意技能的**显示名和 id 不是一回事**（id "launcher" 显示名是 "launchers"，
//  译作「重武器」而不是「发射器」；id "gun" 显示名是 "marksmanship"，译作
//  「枪法」）。照 id 直译会和游戏界面对不上。
inline std::string skill( const std::string &key )
{
    if( key == "rifle" )    return "步枪";
    if( key == "pistol" )   return "手枪";
    if( key == "shotgun" )  return "霰弹枪";
    if( key == "smg" )      return "冲锋枪";
    if( key == "launcher" ) return "重武器";
    if( key == "archery" )  return "弓术";
    if( key == "gun" )      return "枪法";
    if( key == "throw" )    return "投掷";
    return key;
}

// 改装槽位名
inline std::string slot( const std::string &key )
{
    if( key == "rail" )            return "导轨";
    if( key == "sights" )          return "瞄具";
    if( key == "underbarrel" )     return "管下";
    if( key == "muzzle" )          return "枪口";
    if( key == "barrel" )          return "枪管";
    if( key == "bore" )            return "口径";
    if( key == "mechanism" )       return "机械";
    if( key == "stock" )           return "枪托";
    if( key == "stock accessory" ) return "枪托配件";
    if( key == "sling" )           return "背带";
    if( key == "brass catcher" )   return "弹壳收集器";
    if( key == "bayonet lug" )     return "刺刀座";
    if( key == "magnifier" )       return "变焦器";
    if( key == "dampening" )       return "减震";
    return key;
}

// 命中档位（missed_by 越小越好）
inline const char *hit_tier( double missed_by )
{
    if( missed_by >= 1.0 ) return "脱靶";
    if( missed_by >= 0.8 ) return "擦伤（伤害5%~25%）";
    if( missed_by >= 0.5 ) return "普通";
    if( missed_by >= 0.2 ) return "好击";
    if( missed_by >= 0.1 ) return "暴击";
    return "爆头";
}

// 瞄准档位名（对应游戏里的 Regular / Careful / Precise）
inline const char *AIM_LEVEL_1 = "普通档";
inline const char *AIM_LEVEL_2 = "仔细档";
inline const char *AIM_LEVEL_3 = "精准档";
inline const char *AIM_LEVEL_0 = "完全没瞄";

// =============================================================================
//  二、标题与表头
// =============================================================================
namespace t {

inline const char *RULE          = "==================================================\n";
inline const char *TITLE         = " 枪械数学模型（公式复刻自游戏源码）\n";
inline const char *TITLE_SUB     = " 参考文件：character.cpp / ranged.cpp / item_gun_tool_ammo.cpp\n";

// ---- 枪械有效数据 ----
inline const char *HDR_GUN       = "\n================= 枪械有效数据 =================\n";
inline const char *LBL_SKILL     = "  技能             : ";
inline const char *LBL_WEIGHT    = "  重量             : ";
inline const char *LBL_VOLUME    = "  体积             : ";
inline const char *LBL_DISP_RAW  = "  枪基础散布(原始) : ";
inline const char *LBL_DISP_REAL = "  枪基础散布(实际) : ";
inline const char *NOTE_DIV18    = "   （原始值 ÷ 18）\n";
inline const char *LBL_SIGHT     = "  铁瞄散布         : ";
inline const char *NOTE_NO_DIV18 = "  （不除以 18）\n";
inline const char *LBL_HANDLING  = "  操控性           : ";
inline const char *LBL_AMMO      = "  弹药             : ";
inline const char *LBL_AMMO_SEP  = "  （后坐 ";
inline const char *LBL_AMMO_SEP2 = "，散布 ";
inline const char *BR_CLOSE      = "）\n";
inline const char *HDR_MODS      = "\n  --- 安装的配件 ---\n";
inline const char *NO_MODS       = "    (无)\n";
inline const char *F_SLOT        = " 槽位=";
inline const char *F_HANDLING    = " 操控+";
inline const char *F_AIM         = " 瞄准+";
inline const char *F_SIGHTDISP   = " 散布=";
inline const char *F_FOV         = " 视野=";

// ---- 瞄准参数 ----
inline const char *HDR_AIMPARAM  = "\n  --- 瞄准参数（人物 敏捷 ";
inline const char *HDR_AIMPARAM2 = " / 感知 ";
inline const char *HDR_AIMPARAM3 = " / 技能 ";
inline const char *HDR_AIMPARAM4 = "）---\n";
inline const char *LBL_HIPLIMIT  = "    腰射极限            : ";
inline const char *LBL_AIMLIMIT  = "    瞄准精度上限        : ";
inline const char *LBL_VOLFACT   = "    体积因子            : ";
inline const char *NOTE_VOLFACT  = "   （体积 >800毫升时再乘 (800÷体积) 的立方根）\n";
inline const char *LBL_LENFACT   = "    长度因子(空旷/贴墙) : ";
inline const char *SEP_SLASH     = " / ";
inline const char *LBL_TOTDISP   = "    总散布(含技能惩罚)  : ";
inline const char *LBL_SHOTREC   = "    单发后坐            : ";
inline const char *NOTE_BIPOD    = "   （两脚架架设时 ";
inline const char *LBL_ADDREC    = "    每发增加的瞄准误差  : ";
inline const char *NOTE_ABSORB   = "   （技能吸收 ";
inline const char *PCT_CLOSE     = "%）\n";

// ---- 瞄准时间线 ----
inline const char *HDR_AIMTIME   = "\n================= 瞄准时间线 =================\n";
inline const char *NOTE_AIMTIME  = "  （从瞄准误差 3000 开始，每消耗 1 点行动力降低一次）\n";
inline const char *NOTE_AIMCHAR  =
    "  当前人物：%s %g 级 / 枪械技能 %g 级 / 敏捷 %g / 感知 %g\n\n";
inline const char *LBL_TURNDROP2 = "  一回合（100 行动点）后降到 ";
inline const char *C_TO_1        = "到普通档";
inline const char *C_TO_2        = "到仔细档";
inline const char *C_TO_3        = "到精准档";
inline const char *U_AP          = " 行动点";
inline const char *LBL_THRESHOLD = "  档位阈值: 普通 ";
inline const char *LBL_THRESH2   = "  /  仔细 ";
inline const char *LBL_THRESH3   = "  /  精准 ";

// ---- 散布 → 命中影响 ----
inline const char *HDR_DISPIMP   = "\n================= 散布 → 命中影响 =================\n";
inline const char *SUB_DISPIMP   = "  散布值 → 50% 好击距离\n\n";
inline const char *OVER_TABLE    = "超出表格（59 格，最大视野）";
inline const char *U_TILE        = " 格";
inline const char *HDR_INSTANCE  = "\n  实例：瞄到各档位后的表现（目标体积按 1.0 格的人形怪计算）\n\n";
inline const char *C_AIMLEVEL    = "瞄准程度";
inline const char *C_RECOIL      = "瞄准误差";
inline const char *C_FIXDISP     = "固定散布";
inline const char *C_TOTDISP     = "总散布";
inline const char *C_50RANGE     = "50%好击距离";
inline const char *NOTE_DISP_COL =
    "  （总散布 = 瞄准误差 + 固定散布。固定散布不随瞄准进度变化，"
    "由枪身+弹药散布 ÷18、敏捷修正、技能不足惩罚三项相加而来）\n";
inline const char *NOTE_HITRULE  =
    "  命中判定：未命中度 = 横向偏移 ÷ 目标体积，"
    "横向偏移 = tan(误差角÷2) × 距离 × 2（源码 ballistics.cpp 第 224 行）\n";

// ---- 装配对话框 ----
inline const char *SLOTS_LINE    =
    "\n  可安装槽位: 导轨 / 瞄具 / 管下 / 枪口 / 枪托 / 枪托配件 / 枪管 / 机械\n";
inline const char *SLOTS_NOTE    =
    "  （M4A1 没有原生的导轨/瞄具/管下/枪口槽位 —— 那些由已安装的 .223 上机匣提供）\n";
inline const char *MODLIST_HDR   =
    "\n  配件列表（输入编号加入；直接回车结束；同槽位会自动替换，因为一个槽位只能装一件）：\n";
inline const char *PROMPT_MODID  = "\n  编号> ";
inline const char *ADDED         = "    + ";
inline const char *REPLACED      = "  (替换了同槽位的旧配件)";
inline const char *OUT_OF_RANGE  = "    编号超出范围\n";
// 装配对话框里的列标签（与上面 F_* 的写法略有不同，故单独列出）
inline const char *DLG_HANDLING  = " 操控";
inline const char *DLG_AIM       = "  瞄准";
inline const char *DLG_DISP      = "  散布";
inline const char *DLG_FOV       = "  视野";

// ---- 模式选择 ----
inline const char *MODE_PROMPT   = "\n请选择功能：\n";
inline const char *MODE_1        = "  1) 搜索并对比枪械\n";
inline const char *MODE_2        = "  2) 单枪详情分析\n";
inline const char *MODE_0        = "  0) 退出\n";

// ---- 搜索 ----
inline const char *SEARCH_PROMPT = "\n输入关键词（匹配中文名 / 英文名 / id / 变体别名）\n  直接回车返回上级：";
inline const char *SEARCH_NONE   = "  没有匹配的枪械。\n";
inline const char *SEARCH_HITS   = "\n  匹配到 %d 把：\n";
inline const char *SEARCH_PICK   =
    "\n  输入编号加入对比列表（可用逗号分隔多个，如 0,3,5）\n  直接回车结束：";
inline const char *SEARCH_ADDED  = "    已加入：";
inline const char *SEARCH_BAD    = "    编号超出范围\n";
inline const char *SEARCH_MORE    = "  ...（还有 %d 把未列出，请用更具体的关键词）\n";

// ---- 游戏内显示值 ----
// 格式与 0.I 稳定版的物品界面一致：分项相加，显示的是原始内部值（不除以 100）
inline const char *HDR_GAMEVAL   =
    "\n  --- 游戏内显示值 ---\n"
    "  （与游戏物品界面显示的数字一致，可随时照着核对）\n";
inline const char *GV_AMMO       = "    弹药                 : ";
inline const char *GV_DISP       = "    散布（枪身+弹药）    : ";
inline const char *GV_DISP_EQ    = " = ";
inline const char *GV_SIGHT      = "    瞄准散布（瞄具+视差）: ";
inline const char *GV_SIGHT_PS   = "    瞄准散布（腰射）     : ";
inline const char *GV_RECOIL     = "    实际后坐             : ";
inline const char *GV_RECOIL_BIP = "    两脚架架设时         : ";
inline const char *GV_THEO       = "    理论最小后坐力       : ";
inline const char *GV_STR_REQ    = "（所需力量: ";
inline const char *GV_STR_END    = "）";
inline const char *GV_PLUS       = "+";
inline const char *NL            = "\n";

// 每一项的作用说明（直接跟在数值下方）
inline const char *NOTE_DISP_1  =
    "        └ 子弹的抖动角度。值越大，同一个距离上越容易从「好击」掉到「擦伤」\n";
inline const char *NOTE_DISP_2  =
    "          误差随距离线性放大：1 格时几乎无差别，20 格外就很明显\n";
inline const char *NOTE_SIGHT_1 =
    "        └ 瞄准能压到的误差下限 —— 瞄得再久，散布也不会低于这个值\n";
inline const char *NOTE_SIGHT_2 =
    "          由瞄具本身散布 + 感知造成的视差（感知越低视差越大）组成\n";
inline const char *NOTE_RECOIL_1 =
    "        └ 每开一枪，给瞄准误差增加 5 倍该值（技能最多吸收一半）\n";
inline const char *NOTE_RECOIL_2 =
    "          误差一旦涨上去就只能靠重新瞄准压回来，这是枪战的主要时间成本\n";
inline const char *NOTE_THEO_1  =
    "        └ 力量补足到「所需力量」后能达到的后坐，是这把枪的下限\n";
inline const char *NOTE_THEO_2  =
    "          与实际后坐相等 = 你的力量已经够了；更大 = 力量不足，后坐被放大\n";

// 瞄准等级 —— 游戏物品界面里每个瞄准档位也会列出这两项
inline const char *HDR_AIMLEVELS = "\n    瞄准等级（游戏物品界面里每档也会列出这两项）\n";
inline const char *LV_AIMLEVEL   = "瞄准等级";
inline const char *LV_50RANGE    = "50%命中距离";
inline const char *LV_AIMTIME    = "瞄准用时";
inline const char *LV_AP         = " 行动点";
inline const char *NOTE_AIMLEVEL =
    "        └ 50%命中距离 = 在这个距离上约有一半概率打出「好击」\n"
    "          瞄准用时   = 从完全没瞄压到这一档要花多少行动点（1 回合 = 100 点）\n";

// ---- 对比表 ----
inline const char *CMP_TITLE     = "\n================= 枪械对比 =================\n";
inline const char *CMP_ASSUME    =
    "  假设：武器技能 %g / 枪械技能 %g / 敏捷 %g / 感知 %g；目标体积 1.0 格\n";
inline const char *CMP_EMPTY     = "  对比列表为空。\n";
inline const char *CMP_HEAD_GUN  = "枪械";
inline const char *CMP_HEAD_SKILL= "技能";
inline const char *CMP_HEAD_AMMO = "弹药";
inline const char *CMP_HEAD_DISP = "枪散布";
inline const char *CMP_HEAD_GDISP= "游戏散布";
inline const char *CMP_HEAD_SIGHT= "瞄具散布";
inline const char *CMP_HEAD_HAND = "操控";
inline const char *CMP_HEAD_W    = "重量";
inline const char *CMP_HEAD_V    = "体积";
inline const char *CMP_HEAD_REC  = "每发+误差";
inline const char *CMP_HEAD_AIM  = "瞄到普通档";
inline const char *CMP_NOTE      =
    "  注：「每发+误差」= 开一枪后瞄准误差的增加量（含枪械技能吸收），"
    "按每把枪各自的标准弹药计算（见「弹药」列）。\n"
    "      不同口径之间这一列不可直接横向比较。\n";
inline const char *CMP_NO_AMMO   = "（无适配弹药）";

// ---- 瞄准收益曲线 ----
inline const char *HDR_CURVE     = "\n================= 瞄准收益曲线 =================\n";
inline const char *NOTE_CURVE    =
    "  （横轴 = 瞄准回合（1 回合 = 100 行动点），纵轴 = 50%好击距离（格）\n"
    "    曲线到顶后就白瞄了 —— 瞄准误差压到精度下限便再也降不下去）\n";
inline const char *C_TURN        = "回合";
inline const char *C_RANGE_AXIS  = " 格";

// ---- 命中档位概率 ----
inline const char *HDR_PROB      = "\n================= 命中档位概率 =================\n";
inline const char *NOTE_PROB     =
    "  （对指定距离的目标开一枪，各命中档位出现的概率。\n"
    "    目标体积按 1.0 格，每档采样 20 万次；结果固定，不会每次运行都变）\n";
inline const char *C_DIST        = "距离";
inline const char *NOTE_PROB2    =
    "  判定阈值（未命中度，越小越准）：爆头 0.1 / 暴击 0.2 / 好击 0.5 / "
    "普通 0.8 / 擦伤 1.0 / 脱靶 ≥1.0\n";

// ---- 主流程 ----
inline const char *PROMPT_PICK   = "\n选择枪械：";   // 注意：pick() 会自行补一个换行
inline const char *PROMPT_SEL    = "  选择> ";
inline const char *HDR_AMMO      = "\n选择弹药：\n";
inline const char *LBL_AMMO_REC  = " 后坐 ";
inline const char *LBL_AMMO_DISP = "   散布 ";
inline const char *HDR_CHAR      =
    "\n---- 人物属性（直接回车使用方括号里的默认值）----\n";
inline const char *P_SKILL_LV    = "技能等级 [0]: ";
inline const char *P_GUNSKILL    = "  枪械技能等级 [0]: ";
inline const char *P_DEX         = "  敏捷 [8]: ";
inline const char *P_PER         = "  感知 [8]: ";
inline const char *P_STR         = "  力量 [8]: ";
inline const char *DONE          = "\n完成。按回车退出。";

} // namespace t

} // namespace zh
