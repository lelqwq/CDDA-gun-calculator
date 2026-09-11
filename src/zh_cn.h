// =============================================================================
//  zh_cn.h  —  gunlab 的全部中文文本
// -----------------------------------------------------------------------------
//  这个文件里只有"给人看的字"。计算逻辑在 gunlab_math.cpp，界面在 src/gui/。
//
//  译名来源：游戏本体 CDDA\lang\mo\zh_CN\LC_MESSAGES\cataclysm-dda.mo
//            （用 Python 的 gettext 提取，见 extract_zh.py）
//
//  想改中文？只动这一个文件（以及 zh::g 下面的界面文案），然后重新编译即可。
//
//  注意：GunMod.location（"underbarrel"）、Gun.skill（"rifle"）、GunMod.id
//  这些英文串是**逻辑用的键**（代码里有 has_mod("underbarrel") 之类的比较，
//  且与游戏 JSON 字段对应），所以不能改成中文 —— 但它们不会出现在输出里，
//  显示时一律经过下面的 skill() / slot() 转换。
//
//  ★ 百分号的两种写法（踩过）：走 note("%s", X) / TextUnformatted(X) 传的是
//    **字面量**，百分号就写一个 %；直接当 printf 格式串用的才写 %%。
//    写错了不报错，只会在界面上多出一个百分号。
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
//
//  译文全部取自游戏 .mo 里对同名 key 的翻译（这些 key 在游戏里也是直接
//  拿去查翻译表的）。数据里实际出现过 26 种，这里覆盖全。
//
//  ★ 注意「底座」那一族：游戏把 rail / sights / stock / underbarrel 这些
//    槽位和它们的 *_mount 版本分得很清楚，前者是枪自带的槽，后者是配件
//    提供的底座。只写 base 名会让 mount 系列漏成英文。
//
//  唯一没有官方译文的是 "launcher"（管下榴弹发射器挂载），游戏自己也是
//  显示英文，所以这里不为它编一个。返回原 key。
inline std::string slot( const std::string &key )
{
    // 枪身自带的槽位
    if( key == "barrel" )          return "枪管";
    if( key == "bore" )            return "口径";
    if( key == "mechanism" )       return "机械";
    if( key == "muzzle" )          return "枪口";
    if( key == "stock" )           return "枪托";
    if( key == "stock accessory" ) return "枪托配件";
    if( key == "sling" )           return "背带";
    if( key == "brass catcher" )   return "弹壳收集器";
    if( key == "bayonet lug" )     return "刺刀座";
    if( key == "dampening" )       return "减震器";
    if( key == "loading port" )    return "装弹口";

    // 配件提供的底座槽位
    if( key == "rail" )            return "导轨";
    if( key == "rail mount" )      return "导轨底座";
    if( key == "sights" )          return "瞄具";
    if( key == "sights mount" )    return "瞄具底座";
    if( key == "underbarrel" )     return "管下";
    if( key == "underbarrel mount" ) return "管下底座";
    if( key == "stock mount" )     return "枪托底座";
    if( key == "grip" )            return "握把";
    if( key == "grip mount" )      return "握把底座";

    // 其余专用槽位
    if( key == "magnifier" )       return "变焦器";
    if( key == "lens" )            return "镜头";
    if( key == "arrow rest" )      return "箭托";
    if( key == "stabilizer" )      return "稳定器";
    if( key == "emitter" )         return "发射器";
    if( key == "belt clip" )       return "皮带扣";
    if( key == "condenser" )       return "冷凝器";
    if( key == "slingshot" )       return "弹弓";
    if( key == "accessories" )     return "配件";
    if( key == "conversion" )      return "改装";
    if( key == "magazine" )        return "弹仓";

    // 带下划线的写法。游戏数据里同时存在 "rail mount" 和 "rail_mount"
    // 两种写法，而且游戏给它们准备了**各自独立**的译文 ——
    // sights_mount 译作「瞄具接口」，sights mount 却译作「瞄具底座」。
    // 看着像游戏自己译岔了，但本项目以对齐游戏界面为准，所以照抄。
    if( key == "rail_mount" )      return "导轨底座";
    if( key == "sights_mount" )    return "瞄具接口";
    if( key == "stock_mount" )     return "枪托底座";
    if( key == "underbarrel_mount" ) return "管下底座";

    return key;
}

// 射击模式名（游戏 JSON 里 modes 的第二个元素）
//
//  译文取自游戏 .mo 对同名 key 的翻译 —— 游戏自己也是拿这串去查表的。
//  有些枪没有 modes，由加载器补默认模式，名字见脚本里的 defmode_name()。
inline std::string mode_name( const std::string &key )
{
    if( key == "semi-auto" )  return "半自动";
    if( key == "auto" )       return "全自动";
    if( key == "burst" )      return "短连发";
    if( key == "single" )     return "单发";
    if( key == "revolver" )   return "左轮手枪";
    if( key == "double" )     return "双发齐射";
    if( key == "manual" )     return "手动";
    if( key == "low auto" )   return "低速全自动";
    if( key == "high auto" )  return "高速全自动";
    return key;               // 例如 "3 rd."，游戏译作「三连发」，查不到就原样显示
}

// 命中档位的名字。参数是「未命中度」换算出来的档位序号，0 最准、5 脱靶。
inline const char *hit_tier_short( int tier )
{
    static const char *names[6] = { "爆头", "暴击", "好击", "普通", "擦伤", "脱靶" };
    return ( tier >= 0 && tier < 6 ) ? names[tier] : "?";
}

// 瞄准档位名（对应游戏里的 Regular / Careful / Precise）
inline const char *AIM_LEVEL_1 = "普通档";
inline const char *AIM_LEVEL_2 = "仔细档";
inline const char *AIM_LEVEL_3 = "精准档";
inline const char *AIM_LEVEL_0 = "完全没瞄";

// =============================================================================
//  二、界面文案（src/gui/）
// =============================================================================
//  走 note("%s", X) / TextUnformatted(X) 传的是**字面量**，百分号写一个 %；
//  直接当 printf 格式串用的（比如带 %d 的）才写 %%。写错了不报错，
//  只会在界面上多出一个百分号。
namespace g {

// ---- 顶部工具栏 ----
inline const char *SEARCH_HINT  = "搜索：中文名 / 英文名 / id / 别名";
inline const char *CLEAR        = "清空";
inline const char *COUNT_FMT    = "%d / %d 把";
inline const char *PICK_GUN     = "从左边选一把枪";

// 技能筛选下拉框。两个都走 fmt_str 格式化，所以百分号写一个
inline const char *SKILL_FILTER = "技能";
inline const char *SKILL_ALL_FMT  = "全部（%d）";
inline const char *SKILL_ITEM_FMT = "%s（%d）";

// 人物参数（详情里所有数字都随这几个值变）
inline const char *P_DEX        = "敏捷";
inline const char *P_PER        = "感知";
inline const char *P_STR        = "力量";
inline const char *P_SKILL      = "武器技能";
inline const char *P_MARKS      = "枪械技能";
inline const char *P_SKILL_FMT  = "%s等级";     // 参数是 zh::skill() 的结果
inline const char *CHAR_HINT    = "人物参数（改动会立刻重算右边所有数值）";

// ---- 列表列头 ----
inline const char *COL_NAME     = "名称";
inline const char *COL_SKILL    = "技能";
inline const char *COL_WEIGHT   = "重量";

// ---- 详情：抬头 ----
inline const char *F_SKILL      = "技能";
inline const char *F_WEIGHT     = "重量";
inline const char *F_VOLUME     = "体积";
inline const char *F_AMMO       = "弹药";
inline const char *NO_AMMO      =
    "无适配弹药 —— 模块化枪械（如 M16）要先装上机匣才确定口径";

// ---- 详情：分区标题 ----
inline const char *SEC_MODS     = "已装配件";
inline const char *SEC_AIMPARAM = "瞄准参数";
inline const char *SEC_GAMEVAL  = "游戏内显示值";
inline const char *SEC_AIMLEVEL = "瞄准等级";
inline const char *SEC_TIMELINE = "瞄准时间线";
inline const char *SEC_INSTANCE = "瞄准档位实例";
inline const char *SEC_CURVE    = "瞄准收益与连射";
inline const char *SEC_PROB     = "命中档位概率";

// ---- 连射（持续射击）----
// 曲线图里每条线的名字
inline const char *LINE_FIRST    = "首次开火";              // 冷启动那条
inline const char *MODE_FMT_1    = "%s持续";                // 一次一发：「半自动持续」
inline const char *MODE_FMT_N    = "%s持续（%d 发）";        // 连发：「全自动持续（4 发）」

inline const char *TIP_TURN      = "%d 回合";
inline const char *SUSTAINED_HINT =
    "「持续」= 反复「瞄这么多回合 → 开火」稳定下来的水平。它比「首次开火」准 —— 战斗刚开始时瞄准误差初值就是满的 3000，之后每轮只需从上一发打完的状态恢复。全自动那条若与「首次开火」重合，说明每轮后坐力都顶到了上限，等于每轮都从零开始";

// 逐发明细表
inline const char *BURST_HDR_FMT  = "本轮 %d 发（两发之间瞄 %d 回合，稳态下开火时误差 %.0f）";
inline const char *BURST_INTERVAL = "逐发明细的间隔";
inline const char *BURST_TURNS    = " 回合";
inline const char *COL_SHOT_NO    = "第几发";
// 「好击及以上」跟距离强相关（1 格约九成、40 格几乎为零），必须标出按多远算的
inline const char *COL_GOODPLUS   = "好击及以上 @10格";
inline const char *SHOT_NO_FMT    = "第 %d 发";
inline const char *BURST_TRUNC    = "（后面还有 %d 发没列出）";
inline const char *BURST_OVER_CAP =
    "注意：连发中途的瞄准误差会超过 3000 —— 上限只在打完一轮时才生效（游戏源码就是这样，命中判定用的是没截断的值）";

// 卡壳警告。★ 游戏里卡壳**不会**中断当前这一轮连发，代价在后面：
//   之后每次想开火，都要先花约一回合清障、瞄准进度清零、还有大概率失败。
// 注意两个占位符都是 %d（传进来的是 (int)），别写成 %.0f
inline const char *JAM_TITLE_FMT = "⚠ 这种弹药（recoil %d）推不动这把枪的循环（需要 %d），开火后会卡壳";
inline const char *JAM_DETAIL =
    "游戏判定：弹药的 recoil 低于枪的 min_cycle_recoil 时，开火之后枪会挂上「Spent casing in chamber」故障。此后每次想开火都要先花约一回合清障，瞄准进度直接清零（瞄准误差被重置回 3000），而且只有 1/7 ~ 1/15 的概率当场修好，修不好这次开火就白费。换一款后坐力够的弹药，否则连射基本没法用";

// 瞄准收益曲线（图形版用 ImDrawList 手绘，不是 ImPlot）
//
// ★ 这个文件里的字符串分两类，别搞混：
//   - 走 note("%s", X) / TextUnformatted(X) / AddText(X) 传的是**字面量**，
//     百分号就写一个 %（比如下面的 CURVE_HINT）；
//   - 当 printf 格式串用的才写 %%，而且**实参类型必须对得上**。
//   第二类尤其小心：格式串是变量时编译器查不了（GCC 只查字面量），
//   类型写错了不报错、只是值变鬼数字。本项目已经踩过两次了。
inline const char *CURVE_HINT   =
    "横轴 = 瞄准回合（1 回合 = 100 行动点），纵轴 = 50%好击距离。曲线到顶后就白瞄了 —— 瞄准误差压到精度下限便再也降不下去。鼠标移到图上可看具体数值";
inline const char *AXIS_TURN    = "瞄准回合";
inline const char *AXIS_RANGE   = "50%好击距离（格）";

// 瞄准时间线里的那张图：纵轴换成瞄准误差本身
inline const char *AXIS_RECOIL   = "瞄准误差";
inline const char *LEGEND_FMT    = "%s %d";     // 图例里的一条：档位名 + 阈值（截断，与上方「档位阈值」那行一致）
// 这一节现在只剩这一句说明 + 一张图。原本还列着「压到X档 N 行动点」三行、
// 「一回合后降到 N」和档位阈值 —— 那些数字与上面「瞄准等级」表里的「瞄准用时」
// 是同一组，档位阈值也已进了图例，删掉免得把同一组数字说三遍。
inline const char *TIMELINE_HINT2 =
    "回合开始时的瞄准误差，每消耗 1 点行动力降低一次，所以从 3000 一路降下来。橙色横线是三个档位的判定阈值 —— 曲线穿过哪条，就说明那一回合刚好压进该档位；压到最下面那条平了就说明到极限了。各档位要花多少行动点见上面的「瞄准等级」表";
inline const char *NO_MODS      = "（未安装任何配件）";

// 配件安装 / 移除
inline const char *ADD_MOD      = "安装配件…";
inline const char *REMOVE_MOD   = "移除";
inline const char *MOD_PICKER   = "选择要安装的配件";
inline const char *NO_COMPAT    =
    "这把枪当前没有可用配件 —— 模块化枪械（如 M16）要先装上机匣，"
    "导轨 / 瞄具 / 管下这些槽位是机匣提供的";
inline const char *MOD_REPLACE  = "一个槽位只能装一件，安装会替换掉该槽位原有的配件";
inline const char *COL_AMMO_MOD = "口径";

// 配件表列头
inline const char *COL_MOD_NAME = "配件";
inline const char *COL_MOD_SLOT = "槽位";
inline const char *COL_HANDLING = "操控";
inline const char *COL_AIM      = "瞄准";
inline const char *COL_SIGHT    = "瞄准散布";
inline const char *COL_FOV      = "视野";

// ---- 瞄准参数 ----
inline const char *VOL_FACTOR   = "体积因子";
inline const char *LEN_FACTOR   = "长度因子（空旷 / 贴墙）";
inline const char *TOTAL_DISP   = "总散布（含技能惩罚）";
inline const char *RECOIL_LIMIT = "瞄准精度上限";
inline const char *ADDED_RECOIL = "每发增加的瞄准误差";
inline const char *NOTE_ADDED   = "开一枪给瞄准误差加的量，技能吸收 %.0f%%";

// ---- 游戏内显示值 ----
inline const char *GV_HINT      =
    "与游戏物品界面显示的数字一致，可随时照着核对（0.I 格式：分项相加）";
inline const char *GV_DISP      = "散布（枪身+弹药）";
inline const char *GV_SIGHT     = "瞄准散布（瞄具+视差）";
inline const char *GV_SIGHT_PS  = "瞄准散布（腰射）";
inline const char *GV_RECOIL    = "实际后坐";
inline const char *GV_BIPOD     = "两脚架架设时";
inline const char *GV_MINREC    = "理论最小后坐力";
inline const char *GV_STR_REQ   = "所需力量 %d";
inline const char *NOTE_DISP    =
    "子弹的抖动角度。误差随距离线性放大：1 格时几乎无差别，20 格外就很明显";
inline const char *NOTE_SIGHT   =
    "瞄准能压到的误差下限 —— 瞄得再久也不会低于它。由瞄具散布 + 感知造成的视差组成";
inline const char *NOTE_RECOIL  =
    "每开一枪给瞄准误差增加 5 倍该值（技能最多吸收一半）。涨上去只能靠重新瞄准压回来";
inline const char *NOTE_MINREC  =
    "力量补足到「所需力量」后能达到的后坐，是这把枪的下限。等于实际后坐说明力量已够";

// ---- 瞄准等级 ----
inline const char *COL_AIMLEVEL = "瞄准等级";
inline const char *COL_50RANGE  = "50%命中距离";
inline const char *COL_AIMTIME  = "瞄准用时";
inline const char *NOTE_AIMLEVEL =
    "50%命中距离 = 该距离上约有一半概率打出「好击」；瞄准用时 = 从完全没瞄压到这一档要花多少行动点（1 回合 = 100 点）";
inline const char *AP           = "%d 行动点";
inline const char *TILE_FMT     = "%d 格";
inline const char *OVER_TABLE   = "59+ 格（视野上限）";

// ---- 瞄准时间线 ----
// 这一节只剩一句说明 + 一张图，文案见上面的 TIMELINE_HINT2。
// （原来的 TO_LEVEL / ONE_TURN / THRESHOLDS / TIMELINE_HINT 已随那批
//   重复数字一起删掉。）

// ---- 瞄准档位实例 ----
inline const char *COL_RECOIL   = "瞄准误差";
inline const char *COL_FIXDISP  = "固定散布";
inline const char *COL_TOTDISP  = "总散布";
inline const char *INSTANCE_HINT =
    "总散布 = 瞄准误差 + 固定散布。固定散布不随瞄准进度变化（枪身+弹药散布 ÷18、敏捷修正、技能不足惩罚三项相加）；目标体积按 1.0 格的人形怪计算";

// ---- 命中档位概率 ----
inline const char *COL_DIST     = "距离";
inline const char *PROB_HINT    =
    "对指定距离的目标开一枪，各命中档位出现的概率。每档采样 20 万次，固定种子，结果不会每次运行都变";
inline const char *PROB_RULE    =
    "判定阈值（未命中度，越小越准）：爆头 0.1 / 暴击 0.2 / 好击 0.5 / 普通 0.8 / 擦伤 1.0 / 脱靶 ≥1.0";
inline const char *PROB_LEVEL   = "%s（瞄准误差 %d）";

} // namespace g

} // namespace zh
