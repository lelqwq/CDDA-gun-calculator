// =============================================================================
//  gunlab.cpp  —  Cataclysm-DDA 枪械数学模型（独立复刻）
// -----------------------------------------------------------------------------
//  用途：不启动游戏，手动输入枪械/弹药/配件/人物数据，直接算出
//        · 后坐力、散布、瞄准速度
//        · 瞄准到各档位需要多少行动点
//        · 一回合能降低多少后坐
//        · 每发开火增加多少后坐
//        · 50% 好击距离
//
//  所有公式逐条复刻自 Cataclysm-DDA 源码，注释里标了出处（文件:行号）。
//  纯标准库，无外部依赖。Visual Studio 里新建「空项目」→ 添加此 .cpp → F5。
//
//  数据单位说明（非常重要）：
//    dispersion / sight_dispersion / recoil 在 JSON 里的单位是「1/100 角分」，
//    但进游戏前 gun_dispersion() 会先除以 GUN_DISPERSION_DIVIDER（=18）。
//    sight_dispersion 不除。本程序忠实复刻这一点。
// =============================================================================

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// ---- Windows 控制台编码 -----------------------------------------------------
// 源文件是 UTF-8，编译器（MSVC 的 /utf-8、或 GCC 默认）会把中文字面量按 UTF-8
// 存进可执行文件。但 Windows 控制台默认用系统代码页（简体中文是 936/GBK）解读
// 输出，于是中文变成「鏋鏁板妯″瀷」这样的乱码。
// 解决办法：启动时把控制台输出代码页切成 UTF-8，两边就一致了。
#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX          // 防止 windows.h 定义 min/max 宏，撞上 std::min/std::max
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include "gun_data.h"     // 数据结构 + 常量 + 内置数据库（数据层）
#include "zh_cn.h"        // 全部中文文本

// 常量（MAX_RECOIL / MAX_SKILL / GUN_DISPERSION_DIVIDER / ACC_* 等）见 gun_data.h。
// 数据结构（Gun / Ammo / GunMod / Character）与数据库见 gun_data.h / gun_data.cpp。

// ---- 前向声明（定义在第 6 部分）---------------------------------------------
// 枪 + 已装配件的合成值：重量、体积、口径都要参与计算，所以公式部分也要用。
static std::vector<std::string> effective_ammo_types( const Gun &g );
static double effective_weight( const Gun &g );
static double effective_volume( const Gun &g );

// 枪管长度插值（定义在第 3.8c 节，但 3.8b 的游戏显示值要用到）
static double effective_barrel_length( const Gun &g );
static double dispersions_considering_length( const Ammo &ammo, double barrel_length_mm );

// =============================================================================
//  第 1 部分：数学工具
// =============================================================================

// cata_utility.cpp:157
static double logarithmic(double t) { return 1.0 / (1.0 + std::exp(-t)); }

// cata_utility.cpp:166 —— 注意：min 处返回 1.0，max 处返回 0.0（递减）
static double logarithmic_range(int mn, int mx, int pos)
{
    const double LOGI_CUTOFF = 4.0;
    const double LOGI_MIN    = logarithmic(-LOGI_CUTOFF);
    const double LOGI_MAX    = logarithmic(+LOGI_CUTOFF);
    const double LOGI_RANGE  = LOGI_MAX - LOGI_MIN;

    if (mn >= mx) return 0.0;
    if (pos <= mn) return 1.0;
    if (pos >= mx) return 0.0;

    const double unit_pos   = double(pos - mn) / double(mx - mn);
    const double scaled_pos = LOGI_CUTOFF - 2.0 * LOGI_CUTOFF * unit_pos;
    return (logarithmic(scaled_pos) - LOGI_MIN) / LOGI_RANGE;
}

// =============================================================================
//  第 2 部分：数据模型
// =============================================================================
//  struct Gun / Ammo / GunMod / Character 与全部常量都搬到了 gun_data.h，
//  内置数据库搬到了 gun_data.cpp。本文件只负责公式与显示。

// =============================================================================
//  第 3 部分：公式实现
// =============================================================================

// ---- 3.1 视差  character.cpp:855 --------------------------------------------
// ranged_per_mod() = max((20 - PER) * 1.2, 0)      character.cpp:5061
static int ranged_per_mod(double per)
{
    return (int)std::max((20.0 - per) * 1.2, 0.0);
}
static int get_character_parallax(double per, double vision, bool zoom)
{
    int p = zoom ? (int)(ranged_per_mod(per) * 0.25) : ranged_per_mod(per);
    // ranged_dispersion_vision_mod: 满视觉时为 0，受损时为正
    p += (int)std::round((1.0 - vision) * 30.0);
    return std::max(p, 0);
}
// character.cpp:850
static double effective_dispersion(double per, double vision, double disp, bool zoom)
{
    return get_character_parallax(per, vision, zoom) + disp;
}

// ---- 3.2 腰射极限  character.cpp:882 ----------------------------------------
static double point_shooting_limit(double skill, bool archery)
{
    double s = std::min(skill, double(MAX_SKILL));
    if (archery) return 30.0 + 220.0 / (1.0 + s);
    return 200.0 - 10.0 * s;
}

// ---- 3.3 瞄准方式速度衰减  character.cpp:864 --------------------------------
static double modified_sight_speed(double aim_speed_modifier, double eff_sight_disp, double recoil)
{
    if (recoil <= eff_sight_disp) return 0.0;
    if (eff_sight_disp < 0)       return 0.0;
    const double att = 1.0 - logarithmic_range((int)eff_sight_disp,
                                               (int)(3.0 * eff_sight_disp + 1.0),
                                               (int)recoil);
    return (10.0 + aim_speed_modifier) * att;
}

// ---- 3.4 瞄准精度上限  character.cpp:968 ------------------------------------
static double most_accurate_aiming_method_limit(const Gun& g, const Character& c)
{
    double limit = point_shooting_limit(c.skill(g.skill), g.skill == "archery");

    if (!g.disable_sights) {
        double iron = effective_dispersion(c.per, c.vision, g.sight_dispersion, false);
        if (limit > iron) limit = iron;
    }
    for (auto& m : g.mods) {
        if (m.field_of_view > 0 && m.sight_dispersion >= 0) {
            limit = std::min(limit, effective_dispersion(c.per, c.vision, m.sight_dispersion, m.zoom));
        }
    }
    return limit;
}

// ---- 3.5 瞄准方式择优  character.cpp:899 ------------------------------------
static double fastest_aiming_method_speed(const Gun& g, const Character& c, double recoil,
                                          double target_range, double target_size_moa, bool visible)
{
    const double skill    = c.skill(g.skill);
    const bool   archery  = (g.skill == "archery");

    // 腰射
    double point_mod = archery ? skill : ((g.skill == "pistol") ? (10.0 + 4.0 * skill) : skill);
    double best = modified_sight_speed(point_mod, point_shooting_limit(skill, archery), recoil);

    // 铁瞄：注意 mod 传的是 0（只有 modified_sight_speed 内部那个 10.0 底数）
    if (!g.disable_sights) {
        const double iron_FOV   = 480.0;
        const double iron_limit = effective_dispersion(c.per, c.vision, g.sight_dispersion, false);
        const double iron_speed = modified_sight_speed(0, iron_limit, recoil);
        if (iron_limit < recoil && iron_speed > best && recoil <= iron_FOV) {
            best = iron_speed;
        }
    }

    // 激光类瞄具的可用性  character.cpp:929
    // 原式：range <= (10 + PER) * max(1 - 光照/120, 0)
    // 本程序假定白天/充足光照，故光照项为 1
    const int base_distance = 10;
    const bool laser_available = visible && (target_range <= (base_distance + c.per));

    // 其它瞄具配件
    for (auto& m : g.mods) {
        if (m.sight_dispersion < 0 || m.field_of_view <= 0) continue;
        if (m.laser_sight && !laser_available) continue;

        const double parallax = m.zoom ? get_character_parallax(c.per, c.vision, true)
                                       : get_character_parallax(c.per, c.vision, false);
        const double e_eff = parallax + m.sight_dispersion;

        double eff_mod = (4.0 * parallax > target_size_moa)
                         ? std::min(0.0, m.aim_speed_modifier)
                         : m.aim_speed_modifier;

        if (e_eff < recoil && recoil <= m.field_of_view) {
            double e_speed = modified_sight_speed(eff_mod, e_eff, recoil);
            if (e_speed > best) best = e_speed;
        }
    }
    return best;
}

// ---- 3.6 体积 / 长度因子  character.cpp:1044 / 1056 -------------------------
static double aim_factor_from_volume(const Gun& g, double volume_ml)
{
    double factor = (g.skill == "pistol") ? 4.0 : 1.0;
    const double min_volume = 800.0;
    if (volume_ml > min_volume) {
        factor *= std::pow(min_volume / volume_ml, 1.0 / 3.0);
    }
    return std::max(factor, 0.2);
}
// 贴墙时才会惩罚长枪；open_area = true 时恒为 1.0
static double aim_factor_from_length(double length_mm, bool enclosed)
{
    double factor = 1.0;
    if (enclosed) {
        factor = 1.0 - (length_mm - 300.0) / 1000.0;
        factor = std::min(factor, 1.0);
    }
    return std::max(factor, 0.2);
}

// ---- 3.7 每 move 能降低多少后坐  character.cpp:1038 -------------------------
struct AimContext {
    double limit        = 0.0;
    double vol_factor   = 1.0;
    double len_factor   = 1.0;
    double limb_mod     = 1.0;
    double enchant      = 1.0;
    double target_range = 10.0;
    double target_size_moa = 60.0;
    bool   visible      = true;
};

static double aim_per_move(const Gun& g, const Character& c, double recoil, const AimContext& ctx)
{
    const double ssm = fastest_aiming_method_speed(g, c, recoil,
                                                   ctx.target_range, ctx.target_size_moa, ctx.visible);
    const double skill = c.skill(g.skill);

    double aim_speed = 10.0;
    aim_speed += ssm;
    aim_speed += 0.25 * std::min(skill, double(MAX_SKILL));   // aim_speed_skill_mod  character_modifier.cpp:280
    aim_speed += (c.dex - 8.0) * 0.5;                          // aim_speed_dex_mod    character_modifier.cpp:292
    aim_speed *= ctx.limb_mod;                                 // aim_speed_mod（握/操/举）
    aim_speed /= std::max(1.0, 2.5 - 0.2 * skill);             // 技能 7.5 以下的重惩罚
    aim_speed *= std::max(recoil / MAX_RECOIL,
                          1.0 - logarithmic_range(0, (int)MAX_RECOIL, (int)recoil));

    const double base_cap = 5.0 + skill + std::max(10.0, 3.0 * skill);
    aim_speed = std::min(aim_speed, base_cap * ctx.vol_factor);
    aim_speed = std::min(aim_speed, base_cap * ctx.len_factor);

    aim_speed *= 2.4;
    aim_speed = std::max(aim_speed, MIN_RECOIL_IMPROVEMENT);
    aim_speed = std::min(aim_speed, recoil - ctx.limit);        // 不能超过瞄具允许的下限
    return aim_speed * ctx.enchant;
}

// item::gun_base_weight()  item.cpp —— 枪自身 + 带口径的配件（上机匣）
// 后坐计算用的是这个，不是整枪重量；枪托/瞄具不参与
static double gun_base_weight(const Gun& g)
{
    double w = g.weight_g;
    for (auto& m : g.mods) {
        if (!m.ammo_modifier.empty()) w += m.weight_g;
    }
    return w;
}

// ---- 3.8 开一枪加多少后坐  item_gun_tool_ammo.cpp:1316 ----------------------
static int gun_recoil(const Gun& g, double arm_str, double ammo_recoil,
                      bool bipod = false, bool ideal_strength = false)
{
    if (ammo_recoil <= 0) return 0;   // 没有弹药就没有后坐（DDA 的后坐全部来自弹药）

    // ★ 注意这里用的是 gun_base_weight 而不是整枪重量：
    //   item.cpp:gun_base_weight() = 枪自身重量 + 带口径的配件（上机匣）的重量，
    //   不含枪托、瞄具等其它配件。
    //   近似：用配件的 weight 代替 integral_weight（上机匣两者相同）。
    const double bw = gun_base_weight(g);
    const double wt = ideal_strength
                    ? bw / 333.0
                    : std::min(bw, arm_str * 333.0) / 333.0;

    double handling = g.handling;
    for (auto& m : g.mods) {
        if (bipod || !m.bipod) handling += m.handling_modifier;
    }
    handling /= 10.0;                                       // JSON 是人为放大的整数
    handling = std::pow(wt, 0.8) * std::pow(handling, 1.2);

    const double qty = g.recoil + ammo_recoil;
    if (handling > 1.0) return (int)(qty / handling);
    return (int)(qty * (1.0 + std::fabs(handling)));
}

// 每发实际加到 Character::recoil 的量  ranged.cpp:1271
// 代码里带了 5.0 的底倍数（注释：Temporarily scale by 5x）
static double added_recoil_per_shot(int qty, double absorb)
{
    return 5.0 * qty * (1.0 - absorb);
}
// 技能最多吸收 50%  ranged.cpp:1165
static double recoil_absorb(double skill)
{
    return std::min(skill, double(MAX_SKILL)) / 20.0;
}

// ---- 3.8b 游戏界面显示的数值  0.I: src/item.cpp:3287/3313/3340 ---------------
// 0.I 稳定版的物品界面显示的是**原始内部值**（不除以 100），且散布是分项相加：
//     散布: <枪身散布>+<弹药散布（按枪管长度插值）> = <总和>
//     瞄准散布: <瞄具散布>+<视差> = <有效散布>
//     实际后坐: <gun_recoil(角色)>
// 这几个函数专门用来和游戏界面对照 —— 数字一样就说明整条链路没出错。
//
// 注：0.J 开发版把这里改成了单值并除以 100（item_info.cpp:1252），
//     所以拿 0.J 的界面数字来比会对不上。gunlab 对齐的是 0.I。
static double game_dispersion_gun(const Gun& g)
{
    // item::gun_dispersion( with_ammo=false, with_scaling=false )
    double v = g.dispersion;
    for (auto& m : g.mods) v += m.dispersion_modifier;
    return std::max(v, 0.0);
}
static double game_dispersion_ammo(const Gun& g, const Ammo* ammo)
{
    if (!ammo) return 0.0;
    return dispersions_considering_length(*ammo, effective_barrel_length(g));
}
// item::sight_dispersion(character) —— 返回 (瞄具自身散布, 含视差的有效散布)
static std::pair<int, int> sight_dispersion_pair(const Gun& g, const Character& c)
{
    int act = g.disable_sights ? 300 : (int)g.sight_dispersion;
    int eff = (int)effective_dispersion(c.per, c.vision, act, false);
    for (auto& m : g.mods) {
        if (m.sight_dispersion < 0 || m.field_of_view <= 0) continue;
        const int e_act = (int)m.sight_dispersion;
        const int e_eff = (int)effective_dispersion(c.per, c.vision, e_act, m.zoom);
        if (eff > e_eff) { eff = e_eff; act = e_act; }
    }
    return std::make_pair(act, eff);
}
static double game_recoil(const Gun& g, const Character& c, const Ammo* ammo)
{
    return gun_recoil(g, c.str, ammo ? ammo->recoil : 0.0);
}
static double game_recoil_bipod(const Gun& g, const Character& c, const Ammo* ammo)
{
    return gun_recoil(g, c.str, ammo ? ammo->recoil : 0.0, true);
}
static double game_min_recoil(const Gun& g, const Character& c, const Ammo* ammo)
{
    return gun_recoil(g, c.str, ammo ? ammo->recoil : 0.0, true, true);
}

// ---- 3.8c 枪管长度插值  cata_utility.cpp:267 / itype.cpp:462 ------------------
// multi_lerp：按 points 的分段线性插值，两端钳制
static double multi_lerp(const std::vector<std::pair<double, double>>& points, double x)
{
    if (points.empty()) return 0.0;
    size_t i = 0;
    while (i < points.size() && points[i].first <= x) i++;
    if (i == 0) return points.front().second;
    if (i >= points.size()) return points.back().second;
    const double t = (x - points[i - 1].first) / (points[i].first - points[i - 1].first);
    return t * points[i].second + (1.0 - t) * points[i - 1].second;
}

// item::barrel_length()：枪自身的枪管长度，没有就找第一个带枪管长度的配件
// （模块化枪械的枪管长度来自上机匣）
static double effective_barrel_length(const Gun& g)
{
    if (g.barrel_length_mm > 0.0) return g.barrel_length_mm;
    for (auto& m : g.mods) {
        if (m.barrel_length_mm > 0.0) return m.barrel_length_mm;
    }
    return 0.0;
}

// islot_ammo::dispersion_considering_length  itype.cpp:462
// 弹药的散布随枪管长度变化：基准 dispersion + 插值修正
static double dispersions_considering_length(const Ammo& ammo, double barrel_length_mm)
{
    if (ammo.disp_by_barrel.empty()) return ammo.dispersion;
    return multi_lerp(ammo.disp_by_barrel, barrel_length_mm) + ammo.dispersion;
}

// ---- 3.9 散布合成 -----------------------------------------------------------
// item::gun_dispersion  item_gun_tool_ammo.cpp:1192
static double gun_dispersion(const Gun& g, const Ammo* ammo,
                             int damage_level = 0, bool with_scaling = true)
{
    double sum = g.dispersion;
    for (auto& m : g.mods) sum += m.dispersion_modifier;
    sum += damage_level * DISPERSION_PER_GUN_DAMAGE;
    sum = std::max(sum, 0.0);
    // 弹药散布按枪管长度插值  itype.cpp:462
    if (ammo) sum += dispersions_considering_length(*ammo, effective_barrel_length(g));

    if (!with_scaling) return sum;
    sum = std::max(std::round(sum / GUN_DISPERSION_DIVIDER), 1.0);
    return sum;
}
// dispersion_from_skill  ranged.cpp:2651
static double dispersion_from_skill(double skill, double weapon_dispersion)
{
    if (skill >= MAX_SKILL) return 0.0;
    const double shortfall = MAX_SKILL - skill;
    double penalty = 10.0 * shortfall;
    const double threshold = 5.0;

    if (skill >= threshold) {
        const double post = MAX_SKILL - skill;
        return penalty + (weapon_dispersion * post * 1.25) / (MAX_SKILL - threshold);
    }
    const double pre = threshold - skill;
    penalty += weapon_dispersion * (1.25 + pre * 10.0 / threshold);
    return penalty;
}
// Character::get_weapon_dispersion（不含后坐）  ranged.cpp:2674
static double get_weapon_dispersion(const Gun& g, const Character& c, const Ammo* ammo)
{
    double d = gun_dispersion(g, ammo);

    d += (c.dex - 8.0) * 1.0;        // ranged_dex_mod（线性源，简化）
    // 手部操作惩罚：健康时为 0
    d += 0.0;

    const double avg = c.avg_skill(g.skill);
    const double ref = (g.skill == "archery") ? 450.0 / GUN_DISPERSION_DIVIDER
                                              : 300.0 / GUN_DISPERSION_DIVIDER;
    d += dispersion_from_skill(avg, ref);
    return d;
}
// 开火时的总散布 = get_weapon_dispersion() + 当前后坐（ranged.cpp:673 的
// Character::total_gun_dispersion）。用到的地方直接在表达式里写，
// 不再单独包一层函数。

// ---- 3.10 50% 好击距离  creature.cpp:3574 / ranged.cpp:662 ------------------
static const int DISP_TABLE[59] = {
    1731, 859, 573, 421, 341, 286, 245, 214, 191, 175,
     151, 143, 129, 118, 114, 107, 101,  94,  90,  78,
      78,  78,  74,  71,  68,  66,  62,  61,  59,  57,
      46,  46,  46,  46,  46,  46,  45,  45,  44,  42,
      41,  41,  39,  39,  38,  37,  36,  35,  34,  34,
      33,  33,  32,  30,  30,  30,  30,  29,  28
};
static int range_with_even_chance_of_good_hit(double dispersion)
{
    int r = 0;
    while (r < 59 && dispersion < DISP_TABLE[r]) r++;
    return r;   // 返回格数；59 = 超出表格
}

// ---- 3.11 命中偏移  line.cpp:21 / ballistics.cpp:224 ------------------------
// iso_tangent(distance, angle) = tan(angle/2) * distance * 2
// 角度单位：角分（代码把 dispersion 直接喂给 from_arcmin，见 ballistics.cpp:224）
static const double PI_CONST = 3.14159265358979323846;
static double iso_tangent(double distance_tiles, double angle_arcmin)
{
    const double angle_rad = (angle_arcmin / 60.0) * (PI_CONST / 180.0);
    return std::tan(angle_rad / 2.0) * distance_tiles * 2.0;
}
// ballistics.cpp:227 —— missed_by 归一化到 0(正中) ~ 1(完全脱靶)
static double missed_by(double total_dispersion, double range_tiles, double target_size)
{
    return std::min(1.0, iso_tangent(range_tiles, total_dispersion) / target_size);
}

// ---- 3.11b 命中档位概率 -----------------------------------------------------
// 复刻 dispersion_sources::roll()（src/dispersion.cpp）：
//   枪身散布是**正态源**：rng_normal(D) = 正态(均值 D/2, 标准差 D/4)，钳制到 [0,D]
//   其余散布源都是**均匀源**：rng_float(0, s)
//   （dispersion_sources 的构造函数把第一个源放进 normal_sources，
//     add_range 加的进 linear_sources —— 见 src/dispersion.h）
// 最后取 min(总和, 3600)。
static double roll_dispersion(const Gun& g, const Character& c, const Ammo* ammo,
                              double recoil, std::mt19937& rng)
{
    double r = 0.0;

    // 正态源：枪身 + 弹药散布（已 ÷18）
    const double gun_disp = gun_dispersion(g, ammo);
    if (gun_disp > 0.0) {
        std::normal_distribution<double> nd(gun_disp / 2.0, gun_disp / 4.0);
        r += std::min(std::max(nd(rng), 0.0), gun_disp);
    }

    // 线性源：敏捷修正、技能不足惩罚、当前瞄准误差
    std::uniform_real_distribution<double> ud(0.0, 1.0);
    const double dexmod = (c.dex - 8.0) * 1.0;
    if (dexmod > 0.0) r += ud(rng) * dexmod;

    const double avg = c.avg_skill(g.skill);
    const double ref = (g.skill == "archery") ? 450.0 / GUN_DISPERSION_DIVIDER
                                              : 300.0 / GUN_DISPERSION_DIVIDER;
    const double sp = dispersion_from_skill(avg, ref);
    if (sp > 0.0) r += ud(rng) * sp;

    if (recoil > 0.0) r += ud(rng) * recoil;

    return std::min(r, 3600.0);
}

// 未命中度 -> 档位序号（0 最好，5 最差）
static int tier_index(double missed_by)
{
    if (missed_by >= ACC_GRAZING)  return 5;   // 脱靶
    if (missed_by >= ACC_STANDARD) return 4;   // 擦伤
    if (missed_by >= ACC_GOODHIT)  return 3;   // 普通
    if (missed_by >= ACC_CRITICAL) return 2;   // 好击
    if (missed_by >= ACC_HEADSHOT) return 1;   // 暴击
    return 0;                                   // 爆头
}

// 命中档位的名称见 zh::hit_tier()（zh_cn.h），判定阈值见上面的 ACC_* 常量。
// tier_index() 用于概率统计时把未命中度分桶。

// -----------------------------------------------------------------------------
//  瞄准模拟
// -----------------------------------------------------------------------------
struct AimResult {
    int    moves_to_regular = -1;
    int    moves_to_careful = -1;
    int    moves_to_precise = -1;
    double regular_th = 0, careful_th = 0, precise_th = 0;
    double recoil_after_1_turn = 0;
    double delta_1_turn        = 0;
};

static AimResult simulate_aim(const Gun& g, const Character& c, AimContext ctx,
                              int turn_moves = 100, int max_moves = 5000)
{
    AimResult res;
    ctx.limit = most_accurate_aiming_method_limit(g, c);
    ctx.vol_factor = aim_factor_from_volume(g, effective_volume(g));
    // ctx.len_factor 由调用方设置

    const double sd = ctx.limit;
    res.precise_th = sd;
    res.careful_th = ((MAX_RECOIL - sd) / 40.0) + sd;
    res.regular_th = ((MAX_RECOIL - sd) / 10.0) + sd;

    double recoil = MAX_RECOIL;
    int moves = 0;
    while (recoil > sd && moves < max_moves) {
        const double amt = aim_per_move(g, c, recoil, ctx);
        if (amt <= 0) break;
        recoil = std::max(sd, recoil - amt);
        moves++;
        if (res.moves_to_regular < 0 && recoil <= res.regular_th) res.moves_to_regular = moves;
        if (res.moves_to_careful < 0 && recoil <= res.careful_th) res.moves_to_careful = moves;
        if (res.moves_to_precise < 0 && recoil <= res.precise_th) res.moves_to_precise = moves;
        if (moves == turn_moves) {
            res.recoil_after_1_turn = recoil;
            res.delta_1_turn        = MAX_RECOIL - recoil;
        }
    }
    if (res.recoil_after_1_turn == 0) {
        res.recoil_after_1_turn = recoil;
        res.delta_1_turn        = MAX_RECOIL - recoil;
    }
    return res;
}

// =============================================================================
//  第 4 部分：内置数据库
// =============================================================================
//  g_guns / g_ammo / g_mods 和 init_database() 都搬到了 gun_data.cpp。
//  想改枪械、弹药、配件的数值 —— 编辑那个文件。

// =============================================================================
//  第 5 部分：输出
// =============================================================================
//  所有中文文本都在 zh_cn.h 里，本文件只负责排版与计算。

// 终端显示宽度：中日韩字符占 2 列，其余占 1 列（按 UTF-8 码点判断，不按字节）
static size_t display_width(const std::string& s)
{
    size_t w = 0;
    for (size_t i = 0; i < s.size(); ) {
        const unsigned char c = (unsigned char)s[i];
        unsigned int cp = 0;
        size_t len = 1;
        if      (c < 0x80)          { cp = c;        len = 1; }
        else if ((c & 0xE0) == 0xC0){ cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0){ cp = c & 0x0F; len = 3; }
        else                        { cp = c & 0x07; len = 4; }
        for (size_t k = 1; k < len && i + k < s.size(); k++)
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);

        const bool wide = (cp >= 0x1100 && cp <= 0x115F)      // 韩文字母
                       || (cp >= 0x2E80 && cp <= 0xA4CF)      // CJK 部首 / 汉字 / 假名
                       || (cp >= 0xAC00 && cp <= 0xD7A3)      // 韩文音节
                       || (cp >= 0xF900 && cp <= 0xFAFF)      // CJK 兼容
                       || (cp >= 0xFF00 && cp <= 0xFF60)      // 全角
                       || (cp >= 0xFFE0 && cp <= 0xFFE6);
        w += wide ? 2 : 1;
        i += len;
    }
    return w;
}
static std::string pad(const std::string& s, size_t n)
{
    std::string r = s;
    size_t w = display_width(s);
    while (w < n) { r += ' '; w++; }
    return r;
}
// 技能名 / 槽位名 / 命中档位的中文转换都在 zh_cn.h 里（zh::skill / zh::slot / zh::hit_tier）

static void print_gun_summary(const Gun& g, const Character& c, const Ammo* ammo)
{
    using namespace zh::t;

    // 只保留枪名、技能、配件 —— 其余内部值不再显示（与游戏界面不是一个体系，
    // 容易被误当成错误）。要看数值就看下面的「游戏内显示值」。
    std::cout << "\n================= " << g.name << " =================\n";
    std::cout << LBL_SKILL << zh::skill(g.skill) << "\n";

    std::cout << HDR_MODS;
    if (g.mods.empty()) std::cout << NO_MODS;
    for (auto& m : g.mods) {
        std::cout << "    " << pad(m.name, 22)
                  << F_SLOT << pad(zh::slot(m.location), 12)
                  << F_HANDLING << m.handling_modifier
                  << F_AIM << m.aim_speed_modifier;
        if (m.sight_dispersion >= 0) std::cout << F_SIGHTDISP << m.sight_dispersion;
        if (m.field_of_view   >= 0) std::cout << F_FOV << m.field_of_view;
        std::cout << "\n";
    }

    std::cout << HDR_AIMPARAM << c.dex << HDR_AIMPARAM2 << c.per
              << HDR_AIMPARAM3 << c.skill_level << HDR_AIMPARAM4;
    // 瞄准精度上限 = 上面的「瞄准散布」，不重复显示
    std::cout << LBL_VOLFACT << std::fixed << std::setprecision(3)
              << aim_factor_from_volume(g, effective_volume(g)) << NOTE_VOLFACT;
    std::cout << LBL_LENFACT << aim_factor_from_length(g.longest_side_mm, false)
              << SEP_SLASH << aim_factor_from_length(g.longest_side_mm, true) << "\n";
    std::cout << LBL_TOTDISP << std::setprecision(1)
              << get_weapon_dispersion(g, c, ammo) << "\n";

    const double ammo_rec = ammo ? ammo->recoil : 0.0;
    const int gr_hip = gun_recoil(g, c.str, ammo_rec, false);
    std::cout << LBL_ADDREC << (int)added_recoil_per_shot(gr_hip, recoil_absorb(c.skill_level))
              << NOTE_ABSORB << std::setprecision(0) << recoil_absorb(c.skill_level) * 100 << PCT_CLOSE;

    // 游戏界面显示值 —— 照着游戏里同一把枪的数值核对（0.I 格式：分项相加）
    std::cout << HDR_GAMEVAL;
    if (ammo) std::cout << GV_AMMO << ammo->name << NL;

    const int d_gun  = (int)game_dispersion_gun(g);
    const int d_ammo = (int)game_dispersion_ammo(g, ammo);
    std::cout << GV_DISP << d_gun << GV_PLUS << d_ammo << GV_DISP_EQ << (d_gun + d_ammo) << NL;
    std::cout << NOTE_DISP_1 << NOTE_DISP_2;

    const std::pair<int, int> sd = sight_dispersion_pair(g, c);
    const int psl = (int)point_shooting_limit(c.skill(g.skill), g.skill == "archery");
    if (psl <= sd.second) {
        std::cout << GV_SIGHT_PS << psl << NL;
    } else {
        std::cout << GV_SIGHT << sd.first << GV_PLUS << (sd.second - sd.first)
                  << GV_DISP_EQ << sd.second << NL;
    }
    std::cout << NOTE_SIGHT_1 << NOTE_SIGHT_2;

    std::cout << GV_RECOIL << (int)game_recoil(g, c, ammo) << NL;
    std::cout << NOTE_RECOIL_1 << NOTE_RECOIL_2;
    if (g.has_mod("underbarrel")) {
        std::cout << GV_RECOIL_BIP << (int)game_recoil_bipod(g, c, ammo) << NL;
    }

    const int min_rec = (int)game_min_recoil(g, c, ammo);
    std::cout << GV_THEO << min_rec
              << GV_STR_REQ << (int)(gun_base_weight(g) / 333.0) << GV_STR_END << NL;
    std::cout << NOTE_THEO_1 << NOTE_THEO_2;

    // 瞄准等级 —— 对应游戏里的 GUN_AIMING_STATS（0.I: item.cpp:3432）
    // 每档列出「50%命中距离」与「瞄准用时」，两者分别来自
    // range_with_even_chance_of_good_hit() 和 gun_engagement_moves()
    AimContext ctx;
    ctx.len_factor = 1.0;
    const AimResult ar = simulate_aim(g, c, ctx);
    const double fixed_disp = get_weapon_dispersion(g, c, ammo);

    std::cout << HDR_AIMLEVELS;
    std::cout << "      " << pad(LV_AIMLEVEL, 10) << pad(LV_50RANGE, 16) << LV_AIMTIME << NL;

    const struct { const char* name; double thr; int mv; } lvs[] = {
        { zh::AIM_LEVEL_1, ar.regular_th, ar.moves_to_regular },
        { zh::AIM_LEVEL_2, ar.careful_th, ar.moves_to_careful },
        { zh::AIM_LEVEL_3, ar.precise_th, ar.moves_to_precise },
    };
    for (auto& lv : lvs) {
        const int rng = range_with_even_chance_of_good_hit(fixed_disp + lv.thr);
        std::cout << "      " << pad(lv.name, 10)
                  << pad(rng >= 59 ? std::string("59+") : (std::to_string(rng) + " 格"), 16)
                  << lv.mv << LV_AP << NL;
    }
    std::cout << NOTE_AIMLEVEL;
}

static void print_aim_timeline(const Gun& g, const Character& c, const Ammo* ammo)
{
    using namespace zh::t;

    (void)ammo;   // 瞄准时间线只由枪/瞄具/人物决定，与弹药无关
    std::cout << HDR_AIMTIME;

    char cbuf[320];
    std::snprintf(cbuf, sizeof(cbuf), NOTE_AIMCHAR,
                  zh::skill(g.skill).c_str(), c.skill_level, c.marksmanship_level, c.dex, c.per);
    std::cout << cbuf << NOTE_AIMTIME;

    AimContext ctx;
    ctx.len_factor = 1.0;
    const AimResult r = simulate_aim(g, c, ctx);

    const struct { const char *label; int mv; } rows[] = {
        { C_TO_1, r.moves_to_regular },
        { C_TO_2, r.moves_to_careful },
        { C_TO_3, r.moves_to_precise },
    };
    for (auto &row : rows) {
        std::cout << "  " << pad(row.label, 12) << row.mv << U_AP << "\n";
    }

    std::cout << "\n" << LBL_TURNDROP2 << (int)r.recoil_after_1_turn << "\n";
    std::cout << LBL_THRESHOLD << (int)r.regular_th
              << LBL_THRESH2 << (int)r.careful_th
              << LBL_THRESH3 << (int)r.precise_th << "\n";
}

static void print_dispersion_impact(const Gun& g, const Character& c, const Ammo* ammo)
{
    using namespace zh::t;

    std::cout << HDR_DISPIMP;
    const int table_size = 59;
    std::cout << SUB_DISPIMP;
    for (double d : {3000.0, 1500.0, 1000.0, 500.0, 300.0, 150.0, 100.0, 60.0, 40.0, 30.0}) {
        int r = range_with_even_chance_of_good_hit(d);
        std::string rs = (r >= table_size) ? OVER_TABLE : (std::to_string(r) + U_TILE);
        std::cout << "    " << pad(std::to_string((int)d), 8) << " → " << rs << "\n";
    }

    const double limit = most_accurate_aiming_method_limit(g, c);
    const struct { const char* label; double recoil; } levels[] = {
        { zh::AIM_LEVEL_0, MAX_RECOIL },
        { zh::AIM_LEVEL_1, ((MAX_RECOIL - limit) / 10.0) + limit },
        { zh::AIM_LEVEL_2, ((MAX_RECOIL - limit) / 40.0) + limit },
        { zh::AIM_LEVEL_3, limit },
    };

    // 「固定散布」= 与瞄准进度无关的那部分 = 枪身+弹药散布(÷18) + 敏捷修正 + 技能惩罚
    const double fixed_disp = get_weapon_dispersion(g, c, ammo);

    std::cout << HDR_INSTANCE;
    std::cout << "  " << pad(C_AIMLEVEL, 12) << pad(C_RECOIL, 12) << pad(C_FIXDISP, 12)
              << pad(C_TOTDISP, 10) << pad(C_50RANGE, 16) << "\n";
    std::cout << "  " << std::string(64, '-') << "\n";
    for (auto& L : levels) {
        const double total = fixed_disp + L.recoil;
        const int rng = range_with_even_chance_of_good_hit(total);
        std::cout << "  " << pad(L.label, 12)
                  << pad(std::to_string((int)L.recoil), 12)
                  << pad(std::to_string((int)fixed_disp), 12)
                  << pad(std::to_string((int)total), 10)
                  << pad(rng >= table_size ? "59 格以上" : (std::to_string(rng) + U_TILE), 16) << "\n";
    }
    std::cout << NOTE_DISP_COL;

    // 「命中档位 vs 距离」那张表已移除 —— 它用的是散布的 max()（最坏情况），
    // 只能给出一个确定性的档位，容易被当成"必然结果"。后面新增的
    // 命中档位概率表给出的是真实分布，信息量更大也更诚实。
}

// ---- 命中档位概率表 ----------------------------------------------------------
// 对每个瞄准档位采样一批散布掷骰，再对每个距离换算成未命中度、统计各档位占比。
// 散布的掷骰与距离无关，所以每档只采样一次，各距离复用同一批样本。
static void print_hit_probabilities(const Gun& g, const Character& c, const Ammo* ammo)
{
    using namespace zh::t;

    const int    N         = 200000;
    const double TARGET    = 1.0;     // 目标体积（格）
    const double distances[] = { 1, 2, 5, 10, 20, 30, 40 };
    const char  *tier_names[6] = { "爆头", "暴击", "好击", "普通", "擦伤", "脱靶" };

    AimContext ctx;
    ctx.len_factor = 1.0;
    const AimResult ar = simulate_aim(g, c, ctx);

    const struct { const char *name; double thr; } lvs[] = {
        { zh::AIM_LEVEL_0, MAX_RECOIL },
        { zh::AIM_LEVEL_1, ar.regular_th },
        { zh::AIM_LEVEL_2, ar.careful_th },
        { zh::AIM_LEVEL_3, ar.precise_th },
    };

    std::cout << HDR_PROB << NOTE_PROB << NOTE_HITRULE;

    std::mt19937 rng( 20260910u );   // 固定种子 —— 结果可复现，方便反复对照
    std::vector<double> samples( N );

    for( const auto &lv : lvs ) {
        for( int i = 0; i < N; i++ ) {
            samples[i] = roll_dispersion( g, c, ammo, lv.thr, rng );
        }

        std::cout << "\n  ── " << lv.name << " ──\n  " << pad( C_DIST, 9 );
        for( const char *t : tier_names ) std::cout << pad( t, 9 );
        std::cout << "\n  " << std::string( 63, '-' ) << "\n";

        for( double d : distances ) {
            long cnt[6] = { 0, 0, 0, 0, 0, 0 };
            for( int i = 0; i < N; i++ ) {
                cnt[tier_index( missed_by( samples[i], d, TARGET ) )]++;
            }
            std::cout << "  " << pad( std::to_string( (int)d ) + " 格", 9 );
            for( int t = 0; t < 6; t++ ) {
                char buf[16];
                std::snprintf( buf, sizeof( buf ), "%.1f%%", 100.0 * cnt[t] / N );
                std::cout << pad( buf, 9 );
            }
            std::cout << "\n";
        }
    }
    std::cout << "\n" << NOTE_PROB2;
}

// print_mod_catalog() 已移除：它不按枪过滤，列的是全部 170 个配件，
// 与装配界面（已按枪过滤并按槽位分组）重复且更有误导性。

// =============================================================================
//  第 6 部分：主程序
// =============================================================================

// ---- 安全输入：读整行；空行/非法输入一律回退到默认值 ----
static std::string read_line(const std::string& prompt, const std::string& def)
{
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return def;
    const size_t a = line.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return def;
    const size_t b = line.find_last_not_of(" \t\r\n");
    return line.substr(a, b - a + 1);
}
static double read_number(const std::string& prompt, double def)
{
    const std::string s = read_line(prompt, "");
    if (s.empty()) return def;
    try { return std::stod(s); } catch (...) { return def; }
}

// ---- 搜索 -------------------------------------------------------------------
// 大小写不敏感的子串匹配（对中文无影响，对英文型号名有用）
static bool icontains(const std::string& hay, const std::string& needle)
{
    if (needle.empty()) return true;
    if (hay.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); i++) {
        bool ok = true;
        for (size_t j = 0; j < needle.size(); j++) {
            if (std::tolower((unsigned char)hay[i + j]) !=
                std::tolower((unsigned char)needle[j])) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}

// 一把枪的全部可搜索名字：id / 中文名 / 英文名 / 变体别名（中英）
static std::vector<std::string> gun_search_keys(const Gun& g)
{
    std::vector<std::string> keys;
    keys.push_back(g.id);
    keys.push_back(g.name);
    keys.push_back(g.name_en);
    for (auto& s : g.aliases)    keys.push_back(s);
    for (auto& s : g.aliases_en) keys.push_back(s);
    return keys;
}

static std::vector<int> search_guns(const std::string& kw)
{
    std::vector<int> hits;
    for (size_t i = 0; i < g_guns.size(); i++) {
        for (auto& k : gun_search_keys(g_guns[i])) {
            if (icontains(k, kw)) { hits.push_back((int)i); break; }
        }
    }
    return hits;
}

// ---- 弹药 -------------------------------------------------------------------
// 枪的有效口径 = 自身声明的 + 已装配件提供的
// （模块化枪械，如 M4A1，自身 ammo 是 NULL，口径由 .223 上机匣提供）
static std::vector<std::string> effective_ammo_types(const Gun& g)
{
    std::vector<std::string> t = g.ammo_types;
    for (auto& m : g.mods) {
        for (auto& a : m.ammo_modifier) {
            if (std::find(t.begin(), t.end(), a) == t.end()) t.push_back(a);
        }
    }
    return t;
}

// 该枪可用的全部弹药
static std::vector<int> ammo_for_gun(const Gun& g)
{
    std::vector<std::string> types = effective_ammo_types(g);
    std::vector<int> out;
    for (size_t i = 0; i < g_ammo.size(); i++) {
        for (auto& t : types) {
            if (g_ammo[i].ammo_type == t) { out.push_back((int)i); break; }
        }
    }
    return out;
}

// 该枪的"标准弹"。选全金属被甲弹（FMJ）—— 它是各口径下最常见的基准弹。
//
// 识别比想象中麻烦：
//   · 口径名和弹药 id 不一定对应（口径 "9x19mm" ↔ id "9mm_fmj"）
//   · 也不是每种口径都有叫 "fmj" 的（762 口径是 "762_jhp" 和 "762_m87"，
//     后者才是被甲弹，用的是南斯拉夫 M87 编号）
// 所以按这个优先级找：id 等于口径 → id 含 fmj / 名称含「被甲」
//   → 排除 jhp / 「空尖」后的第一个 → 该口径下第一个
static const Ammo* pick_default_ammo(const Gun& g)
{
    for (auto& t : effective_ammo_types(g)) {
        const Ammo* first   = nullptr;
        const Ammo* fmj     = nullptr;
        const Ammo* non_jhp = nullptr;
        for (auto& a : g_ammo) {
            if (a.ammo_type != t) continue;
            if (!first) first = &a;
            if (a.id == t) return &a;

            const bool is_jhp = icontains(a.id, "jhp") || a.name.find("空尖") != std::string::npos;
            if (!is_jhp) {
                if (!non_jhp) non_jhp = &a;
                if (!fmj && (icontains(a.id, "fmj") || a.name.find("被甲") != std::string::npos)) {
                    fmj = &a;
                }
            }
        }
        if (fmj)     return fmj;
        if (non_jhp) return non_jhp;
        if (first)   return first;
    }
    return nullptr;
}

// ---- 枪的"类型"：用于匹配配件的 mod_targets  item_gun_tool_ammo.cpp:1136 ------
static std::string gun_type_of(const Gun& g)
{
    if (g.skill == "archery") {
        for (auto& t : g.ammo_types) if (icontains(t, "bolt")) return "crossbow";
        return "bow";
    }
    return g.skill;
}

// 当前实际可用的槽位 = 枪自带 + 已装配件解锁的
// （模块化枪械如 M4A1 的 rail/sights/muzzle 由上机匣的 add_mod 提供）
static std::vector<std::string> available_slots(const Gun& g)
{
    std::vector<std::string> s = g.mod_slots;
    for (auto& m : g.mods) {
        for (auto& a : m.add_mod) {
            if (std::find(s.begin(), s.end(), a) == s.end()) s.push_back(a);
        }
    }
    return s;
}

// 配件能否装在这把枪上  item_gun_tool_ammo.cpp:3066
//   槽位要对得上，且 mod_targets 里要有这把枪的"类型"或它的具体 id
static bool mod_fits_gun(const Gun& g, const GunMod& m)
{
    const std::vector<std::string> slots = available_slots(g);
    if (std::find(slots.begin(), slots.end(), m.location) == slots.end()) return false;
    if (m.mod_targets.empty()) return false;
    const std::string gt = gun_type_of(g);
    for (auto& t : m.mod_targets) {
        if (t == gt || t == g.id) return true;
    }
    return false;
}

// 枪 + 已装配件 的有效重量 / 体积（游戏的 item 会累加配件）
static double effective_weight(const Gun& g)
{
    double w = g.weight_g;
    for (auto& m : g.mods) w += m.weight_g;
    return w;
}
static double effective_volume(const Gun& g)
{
    double v = g.volume_ml;
    for (auto& m : g.mods) v += m.volume_ml;
    return v;
}

// ---- 装配界面：只列兼容配件，按槽位分组 --------------------------------------
// （同槽位的配件挨在一起，一眼看出哪几个在抢同一个位置）
static void equip_dialog(Gun& g)
{
    using namespace zh::t;

    while (true) {
        // 收集兼容配件，并建立"显示编号 -> g_mods 下标"的映射
        std::vector<int> shown;
        for (size_t i = 0; i < g_mods.size(); i++) {
            if (mod_fits_gun(g, g_mods[i])) shown.push_back((int)i);
        }

        const std::vector<std::string> slots = available_slots(g);
        std::cout << SLOTS_LINE;
        if (shown.empty()) {
            std::cout << "  （这把枪当前没有可用配件）\n";
            return;
        }

        std::cout << "\n  可选配件（共 " << shown.size() << " 个，按槽位分组；"
                     "输入编号加入，同槽位自动替换）：\n";
        // ★ display[n] = 第 n 个"显示出来的"配件在 g_mods 里的下标
        //   显示顺序按槽位分组，与 shown 的顺序不同，必须单独记录，
        //   否则用户输入的编号会选到错误的配件。
        std::vector<int> display;
        for (auto& slot : slots) {
            bool head = false;
            for (int gi : shown) {
                if (g_mods[gi].location != slot) continue;
                if (!head) { std::cout << "\n  ── " << zh::slot(slot) << " ──\n"; head = true; }
                const GunMod& m = g_mods[gi];
                std::cout << "    " << pad(std::to_string(display.size()), 4) << pad(m.name, 26)
                          << DLG_HANDLING << std::showpos << (int)m.handling_modifier << std::noshowpos
                          << DLG_AIM << std::showpos << (int)m.aim_speed_modifier << std::noshowpos;
                if (m.sight_dispersion >= 0) std::cout << DLG_DISP << (int)m.sight_dispersion;
                if (m.field_of_view   >= 0) std::cout << DLG_FOV << (int)m.field_of_view;
                if (!m.ammo_modifier.empty()) std::cout << "  " << m.ammo_modifier[0];
                std::cout << "\n";
                display.push_back(gi);
            }
        }

        const std::string s = read_line(PROMPT_MODID, "");
        if (s.empty()) break;
        int v;
        try { v = std::stoi(s); } catch (...) { break; }
        if (v < 0) break;
        if (v < (int)display.size()) {
            const GunMod nm = g_mods[display[v]];
            const size_t before = g.mods.size();
            g.mods.erase(std::remove_if(g.mods.begin(), g.mods.end(),
                         [&](const GunMod& x){ return x.location == nm.location; }), g.mods.end());
            const bool replaced = (g.mods.size() != before);
            g.mods.push_back(nm);
            std::cout << ADDED << nm.name;
            if (replaced) std::cout << REPLACED;
            std::cout << "\n";
        } else {
            std::cout << OUT_OF_RANGE;
        }
    }
}

// ---- 人物属性 ---------------------------------------------------------------
static Character ask_character()
{
    using namespace zh::t;
    Character ch;
    std::cout << HDR_CHAR;
    ch.skill_level        = read_number("  武器技能等级 [0]: ", 0);
    ch.marksmanship_level = read_number(P_GUNSKILL, 0);
    ch.dex                = read_number(P_DEX, 8);
    ch.per                = read_number(P_PER, 8);
    ch.str                = read_number(P_STR, 8);
    ch.skill_level        = std::max(0.0, std::min(ch.skill_level, double(MAX_SKILL)));
    ch.marksmanship_level = std::max(0.0, std::min(ch.marksmanship_level, double(MAX_SKILL)));
    if (ch.dex < 1) ch.dex = 8;
    if (ch.per < 1) ch.per = 8;
    if (ch.str < 1) ch.str = 8;
    return ch;
}

// ---- 搜索选枪：返回下标，用户取消返回 -1 -------------------------------------
static const int MAX_SHOW = 30;

static int ui_pick_gun()
{
    using namespace zh::t;
    char buf[256];
    while (true) {
        std::cout << SEARCH_PROMPT;
        const std::string kw = read_line("", "");
        if (kw.empty()) return -1;

        const std::vector<int> hits = search_guns(kw);
        if (hits.empty()) { std::cout << SEARCH_NONE; continue; }

        std::snprintf(buf, sizeof(buf), SEARCH_HITS, (int)hits.size());
        std::cout << buf;
        const int n = std::min((int)hits.size(), MAX_SHOW);
        for (int i = 0; i < n; i++)
            std::cout << "    " << pad(std::to_string(i), 4)
                      << pad(g_guns[hits[i]].name, 30) << "  " << g_guns[hits[i]].id << "\n";
        if ((int)hits.size() > n) {
            std::snprintf(buf, sizeof(buf), SEARCH_MORE, (int)hits.size() - n);
            std::cout << buf;
        }
        const int v = (int)read_number(PROMPT_SEL, -1);
        if (v >= 0 && v < n) return hits[v];
        std::cout << SEARCH_BAD;
    }
}

// ---- 选弹药（只列该枪可用的）-------------------------------------------------
static const Ammo* ui_pick_ammo(const Gun& g)
{
    using namespace zh::t;
    const std::vector<int> list = ammo_for_gun(g);
    if (list.empty()) {
        std::cout << CMP_NO_AMMO << "\n";
        return nullptr;
    }
    std::cout << HDR_AMMO;
    for (size_t i = 0; i < list.size(); i++) {
        const Ammo& a = g_ammo[list[i]];
        std::cout << "    " << pad(std::to_string(i), 4) << pad(a.name, 26)
                  << LBL_AMMO_REC << (int)a.recoil << LBL_AMMO_DISP << (int)a.dispersion << "\n";
    }
    int v = (int)read_number(PROMPT_SEL, 0);
    if (v < 0 || v >= (int)list.size()) v = 0;
    return &g_ammo[list[v]];
}

// ---- 对比表 ------------------------------------------------------------------
static void print_compare_table(const std::vector<int>& sel, const Character& base_ch)
{
    using namespace zh::t;
    if (sel.empty()) { std::cout << CMP_EMPTY; return; }

    std::cout << CMP_TITLE;
    char buf[256];
    std::snprintf(buf, sizeof(buf), CMP_ASSUME, base_ch.skill_level,
                  base_ch.marksmanship_level, base_ch.dex, base_ch.per);
    std::cout << buf;

    std::cout << "\n  " << pad(CMP_HEAD_GUN, 24) << pad(CMP_HEAD_SKILL, 8)
              << pad(CMP_HEAD_AMMO, 22) << pad(CMP_HEAD_DISP, 8) << pad(CMP_HEAD_GDISP, 10)
              << pad(CMP_HEAD_SIGHT, 10) << pad(CMP_HEAD_HAND, 6)
              << pad(CMP_HEAD_W, 10) << pad(CMP_HEAD_V, 10)
              << pad(CMP_HEAD_REC, 11) << CMP_HEAD_AIM << "\n";
    std::cout << "  " << std::string(132, '-') << "\n";

    for (int gi : sel) {
        const Gun& g = g_guns[gi];
        const Ammo* a = pick_default_ammo(g);

        // 每把枪的技能名不同，所以按枪覆盖 gun_skill
        Character ch = base_ch;
        ch.gun_skill = g.skill;

        AimContext ctx; ctx.len_factor = 1.0;
        const AimResult r = simulate_aim(g, ch, ctx);

        const std::string rec = a
            ? std::to_string((int)added_recoil_per_shot(gun_recoil(g, ch.str, a->recoil),
                                                        recoil_absorb(ch.skill_level)))
            : "—";

        const int gdisp = (int)(game_dispersion_gun(g) + game_dispersion_ammo(g, a));

        std::cout << "  " << pad(g.name, 24) << pad(zh::skill(g.skill), 8)
                  << pad(a ? a->name : std::string(CMP_NO_AMMO), 22)
                  << pad(std::to_string((int)gun_dispersion(g, a)), 8)
                  << pad(std::to_string(gdisp), 10)
                  << pad(std::to_string((int)g.sight_dispersion), 10)
                  << pad(std::to_string((int)g.handling), 6)
                  << pad(std::to_string((int)effective_weight(g)) + " g", 10)
                  << pad(std::to_string((int)effective_volume(g)) + " ml", 10)
                  << pad(rec, 11)
                  << (r.moves_to_regular >= 0 ? std::to_string(r.moves_to_regular) + " 行动点" : "—")
                  << "\n";
    }
    std::cout << CMP_NOTE;
}

// ---- 界面：对比模式 ----------------------------------------------------------
static void ui_compare()
{
    using namespace zh::t;
    Character ch = ask_character();
    std::vector<int> sel;
    char buf[256];
    while (true) {
        std::cout << SEARCH_PROMPT;
        const std::string kw = read_line("", "");
        if (kw.empty()) break;

        const std::vector<int> hits = search_guns(kw);
        if (hits.empty()) { std::cout << SEARCH_NONE; continue; }

        std::snprintf(buf, sizeof(buf), SEARCH_HITS, (int)hits.size());
        std::cout << buf;
        const int n = std::min((int)hits.size(), MAX_SHOW);
        for (int i = 0; i < n; i++)
            std::cout << "    " << pad(std::to_string(i), 4)
                      << pad(g_guns[hits[i]].name, 30) << "  " << g_guns[hits[i]].id << "\n";
        if ((int)hits.size() > n) {
            std::snprintf(buf, sizeof(buf), SEARCH_MORE, (int)hits.size() - n);
            std::cout << buf;
        }

        std::cout << SEARCH_PICK;
        const std::string picks = read_line("", "");
        if (picks.empty()) continue;

        // 解析逗号分隔的编号
        std::stringstream ss(picks);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            try {
                const int v = std::stoi(tok);
                if (v >= 0 && v < n) {
                    const int gi = hits[v];
                    if (std::find(sel.begin(), sel.end(), gi) == sel.end()) {
                        sel.push_back(gi);
                        std::cout << SEARCH_ADDED << g_guns[gi].name << "\n";
                    }
                } else {
                    std::cout << SEARCH_BAD;
                }
            } catch (...) { /* 忽略无法解析的片段 */ }
        }
    }
    print_compare_table(sel, ch);
}

// ---- 界面：单枪详情 ----------------------------------------------------------
static void ui_detail()
{
    using namespace zh::t;
    const int gi = ui_pick_gun();
    if (gi < 0) return;

    Gun gun = g_guns[gi];
    equip_dialog(gun);

    const Ammo* ammo = ui_pick_ammo(gun);
    if (!ammo) {
        std::cout << "  （这把枪没有可用弹药，可能需要在游戏里先装上机匣）\n";
        return;
    }

    Character ch = ask_character();
    ch.gun_skill = gun.skill;

    print_gun_summary(gun, ch, ammo);
    print_aim_timeline(gun, ch, ammo);
    print_dispersion_impact(gun, ch, ammo);
    print_hit_probabilities(gun, ch, ammo);
    // 配件库不再输出 —— 装配界面已按枪过滤并分组，全量列表没有参考价值
}

// =============================================================================
//  主程序
// =============================================================================

int main()
{
#ifdef _WIN32
    // 让控制台按 UTF-8 解读本程序的中文输出（否则显示为乱码）
    SetConsoleOutputCP(CP_UTF8);
#endif

    init_database();

    using namespace zh::t;

    std::cout << RULE << TITLE << TITLE_SUB << RULE;
    std::cout << "  数据库：" << g_guns.size() << " 把枪 / "
              << g_ammo.size() << " 种弹药 / " << g_mods.size() << " 个配件\n";

    while (true) {
        std::cout << MODE_PROMPT << MODE_1 << MODE_2 << MODE_0;
        const int m = (int)read_number(PROMPT_SEL, 0);
        if (m == 1)      ui_compare();
        else if (m == 2) ui_detail();
        else             break;
    }

    std::cout << DONE;
    std::string dummy;
    std::getline(std::cin, dummy);
    return 0;
}
