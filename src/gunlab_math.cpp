// =============================================================================
//  gunlab_math.cpp  —  纯计算逻辑（gunlab_core 的一部分）
// -----------------------------------------------------------------------------
//  这个文件里**没有任何输入输出**：所有函数都是拿 Gun / Ammo / GunMod /
//  Character 算出一个数值或结构体。命令行版和图形界面版共用它，保证两边
//  的算出来的数字永远一致。
//
//  所有公式逐条复刻自 Cataclysm-DDA 源码，注释里标了出处（文件:行号）。
//  基准版本是 **0.I 稳定版**（27939e2）—— 与游戏本体对齐，不是 master。
//
//  对应的声明在 gunlab_math.h。
// =============================================================================

#include "gunlab_math.h"

#include <algorithm>
#include <cmath>
#include <random>

// =============================================================================
//  第 1 部分：数学工具
// =============================================================================

// cata_utility.cpp:157
double logarithmic(double t) { return 1.0 / (1.0 + std::exp(-t)); }

// cata_utility.cpp:166 —— 注意：min 处返回 1.0，max 处返回 0.0（递减）
double logarithmic_range(int mn, int mx, int pos)
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
int ranged_per_mod(double per)
{
    return (int)std::max((20.0 - per) * 1.2, 0.0);
}
int get_character_parallax(double per, double vision, bool zoom)
{
    int p = zoom ? (int)(ranged_per_mod(per) * 0.25) : ranged_per_mod(per);
    // ranged_dispersion_vision_mod: 满视觉时为 0，受损时为正
    p += (int)std::round((1.0 - vision) * 30.0);
    return std::max(p, 0);
}
// character.cpp:850
double effective_dispersion(double per, double vision, double disp, bool zoom)
{
    return get_character_parallax(per, vision, zoom) + disp;
}

// ---- 3.2 腰射极限  character.cpp:882 ----------------------------------------
double point_shooting_limit(double skill, bool archery)
{
    double s = std::min(skill, double(MAX_SKILL));
    if (archery) return 30.0 + 220.0 / (1.0 + s);
    return 200.0 - 10.0 * s;
}

// ---- 3.3 瞄准方式速度衰减  character.cpp:864 --------------------------------
double modified_sight_speed(double aim_speed_modifier, double eff_sight_disp, double recoil)
{
    if (recoil <= eff_sight_disp) return 0.0;
    if (eff_sight_disp < 0)       return 0.0;
    const double att = 1.0 - logarithmic_range((int)eff_sight_disp,
                                               (int)(3.0 * eff_sight_disp + 1.0),
                                               (int)recoil);
    return (10.0 + aim_speed_modifier) * att;
}

// ---- 3.4 瞄准精度上限  character.cpp:968 ------------------------------------
double most_accurate_aiming_method_limit(const Gun& g, const Character& c)
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
double fastest_aiming_method_speed(const Gun& g, const Character& c, double recoil,
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
double aim_factor_from_volume(const Gun& g, double volume_ml)
{
    double factor = (g.skill == "pistol") ? 4.0 : 1.0;
    const double min_volume = 800.0;
    if (volume_ml > min_volume) {
        factor *= std::pow(min_volume / volume_ml, 1.0 / 3.0);
    }
    return std::max(factor, 0.2);
}
// 贴墙时才会惩罚长枪；open_area = true 时恒为 1.0
double aim_factor_from_length(double length_mm, bool enclosed)
{
    double factor = 1.0;
    if (enclosed) {
        factor = 1.0 - (length_mm - 300.0) / 1000.0;
        factor = std::min(factor, 1.0);
    }
    return std::max(factor, 0.2);
}

// ---- 3.7 每 move 能降低多少后坐  character.cpp:1038 -------------------------

double aim_per_move(const Gun& g, const Character& c, double recoil, const AimContext& ctx)
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
double gun_base_weight(const Gun& g)
{
    double w = g.weight_g;
    for (auto& m : g.mods) {
        if (!m.ammo_modifier.empty()) w += m.weight_g;
    }
    return w;
}

// ---- 3.8 开一枪加多少后坐  item_gun_tool_ammo.cpp:1316 ----------------------
int gun_recoil(const Gun& g, double arm_str, double ammo_recoil,
                      bool bipod, bool ideal_strength)
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
double added_recoil_per_shot(int qty, double absorb)
{
    return 5.0 * qty * (1.0 - absorb);
}
// 技能最多吸收 50%  ranged.cpp:1165
double recoil_absorb(double skill)
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
double game_dispersion_gun(const Gun& g)
{
    // item::gun_dispersion( with_ammo=false, with_scaling=false )
    double v = g.dispersion;
    for (auto& m : g.mods) v += m.dispersion_modifier;
    return std::max(v, 0.0);
}
double game_dispersion_ammo(const Gun& g, const Ammo* ammo)
{
    if (!ammo) return 0.0;
    return dispersions_considering_length(*ammo, effective_barrel_length(g));
}
// item::sight_dispersion(character) —— 返回 (瞄具自身散布, 含视差的有效散布)
std::pair<int, int> sight_dispersion_pair(const Gun& g, const Character& c)
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
double game_recoil(const Gun& g, const Character& c, const Ammo* ammo)
{
    return gun_recoil(g, c.str, ammo ? ammo->recoil : 0.0);
}
double game_recoil_bipod(const Gun& g, const Character& c, const Ammo* ammo)
{
    return gun_recoil(g, c.str, ammo ? ammo->recoil : 0.0, true);
}
double game_min_recoil(const Gun& g, const Character& c, const Ammo* ammo)
{
    return gun_recoil(g, c.str, ammo ? ammo->recoil : 0.0, true, true);
}

// ---- 3.8c 枪管长度插值  cata_utility.cpp:267 / itype.cpp:462 ------------------
// multi_lerp：按 points 的分段线性插值，两端钳制
double multi_lerp(const std::vector<std::pair<double, double>>& points, double x)
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
double effective_barrel_length(const Gun& g)
{
    if (g.barrel_length_mm > 0.0) return g.barrel_length_mm;
    for (auto& m : g.mods) {
        if (m.barrel_length_mm > 0.0) return m.barrel_length_mm;
    }
    return 0.0;
}

// islot_ammo::dispersion_considering_length  itype.cpp:462
// 弹药的散布随枪管长度变化：基准 dispersion + 插值修正
double dispersions_considering_length(const Ammo& ammo, double barrel_length_mm)
{
    if (ammo.disp_by_barrel.empty()) return ammo.dispersion;
    return multi_lerp(ammo.disp_by_barrel, barrel_length_mm) + ammo.dispersion;
}

// ---- 3.9 散布合成 -----------------------------------------------------------
// item::gun_dispersion  item_gun_tool_ammo.cpp:1192
double gun_dispersion(const Gun& g, const Ammo* ammo,
                             int damage_level, bool with_scaling)
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
double dispersion_from_skill(double skill, double weapon_dispersion)
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
double get_weapon_dispersion(const Gun& g, const Character& c, const Ammo* ammo)
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
int range_with_even_chance_of_good_hit(double dispersion)
{
    int r = 0;
    while (r < 59 && dispersion < DISP_TABLE[r]) r++;
    return r;   // 返回格数；59 = 超出表格
}

// ---- 3.11 命中偏移  line.cpp:21 / ballistics.cpp:224 ------------------------
// iso_tangent(distance, angle) = tan(angle/2) * distance * 2
// 角度单位：角分（代码把 dispersion 直接喂给 from_arcmin，见 ballistics.cpp:224）
static const double PI_CONST = 3.14159265358979323846;
double iso_tangent(double distance_tiles, double angle_arcmin)
{
    const double angle_rad = (angle_arcmin / 60.0) * (PI_CONST / 180.0);
    return std::tan(angle_rad / 2.0) * distance_tiles * 2.0;
}
// ballistics.cpp:227 —— missed_by 归一化到 0(正中) ~ 1(完全脱靶)
double missed_by(double total_dispersion, double range_tiles, double target_size)
{
    return std::min(1.0, iso_tangent(range_tiles, total_dispersion) / target_size);
}

// ---- 确定性随机 --------------------------------------------------------------
// std::mt19937 的输出序列是标准规定的，但 std::uniform_real_distribution 和
// std::normal_distribution 的**算法各标准库实现不同** —— 同一颗种子在
// libstdc++（GCC）和 MSVC STL 下会抽出不同样本，导致概率表小数点后一位对不上。
// 为了让结果真正可复现（输出里承诺了「结果固定」），这里自己做映射。
namespace {

// [0,1) 均匀分布：取 mt19937 输出的高 24 位，正好是 double 的尾数位数
double rand01( std::mt19937 &rng )
{
    return ( rng() >> 8 ) * ( 1.0 / 16777216.0 );
}

// 正态分布：Box-Muller 变换（极坐标形式，避免缓存备用样本）
double normal_sample( double mean, double stddev, std::mt19937 &rng )
{
    double u, v, s;
    do {
        u = rand01( rng ) * 2.0 - 1.0;
        v = rand01( rng ) * 2.0 - 1.0;
        s = u * u + v * v;
    } while( s >= 1.0 || s == 0.0 );
    return mean + stddev * u * std::sqrt( -2.0 * std::log( s ) / s );
}

} // namespace

// ---- 3.11b 命中档位概率 -----------------------------------------------------
// 复刻 dispersion_sources::roll()（src/dispersion.cpp）：
//   枪身散布是**正态源**：rng_normal(D) = 正态(均值 D/2, 标准差 D/4)，钳制到 [0,D]
//   其余散布源都是**均匀源**：rng_float(0, s)
//   （dispersion_sources 的构造函数把第一个源放进 normal_sources，
//     add_range 加的进 linear_sources —— 见 src/dispersion.h）
// 最后取 min(总和, 3600)。
double roll_dispersion(const Gun& g, const Character& c, const Ammo* ammo,
                              double recoil, std::mt19937& rng)
{
    double r = 0.0;

    // 正态源：枪身 + 弹药散布（已 ÷18）
    const double gun_disp = gun_dispersion(g, ammo);
    if (gun_disp > 0.0) {
        r += std::min(std::max(normal_sample(gun_disp / 2.0, gun_disp / 4.0, rng), 0.0),
                      gun_disp);
    }

    // 线性源：敏捷修正、技能不足惩罚、当前瞄准误差
    const double dexmod = (c.dex - 8.0) * 1.0;
    if (dexmod > 0.0) r += rand01(rng) * dexmod;

    const double avg = c.avg_skill(g.skill);
    const double ref = (g.skill == "archery") ? 450.0 / GUN_DISPERSION_DIVIDER
                                              : 300.0 / GUN_DISPERSION_DIVIDER;
    const double sp = dispersion_from_skill(avg, ref);
    if (sp > 0.0) r += rand01(rng) * sp;

    if (recoil > 0.0) r += rand01(rng) * recoil;

    return std::min(r, 3600.0);
}

// 未命中度 -> 档位序号（0 最好，5 最差）
int tier_index(double missed_by)
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

AimResult simulate_aim(const Gun& g, const Character& c, AimContext ctx,
                              int turn_moves, int max_moves)
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

// ---- 搜索 -------------------------------------------------------------------
// 大小写不敏感的子串匹配（对中文无影响，对英文型号名有用）
bool icontains(const std::string& hay, const std::string& needle)
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
std::vector<std::string> gun_search_keys(const Gun& g)
{
    std::vector<std::string> keys;
    keys.push_back(g.id);
    keys.push_back(g.name);
    keys.push_back(g.name_en);
    for (auto& s : g.aliases)    keys.push_back(s);
    for (auto& s : g.aliases_en) keys.push_back(s);
    return keys;
}

std::vector<int> search_guns(const std::string& kw)
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
std::vector<std::string> effective_ammo_types(const Gun& g)
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
std::vector<int> ammo_for_gun(const Gun& g)
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
const Ammo* pick_default_ammo(const Gun& g)
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
std::string gun_type_of(const Gun& g)
{
    if (g.skill == "archery") {
        for (auto& t : g.ammo_types) if (icontains(t, "bolt")) return "crossbow";
        return "bow";
    }
    return g.skill;
}

// 当前实际可用的槽位 = 枪自带 + 已装配件解锁的
// （模块化枪械如 M4A1 的 rail/sights/muzzle 由上机匣的 add_mod 提供）
std::vector<std::string> available_slots(const Gun& g)
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
bool mod_fits_gun(const Gun& g, const GunMod& m)
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
double effective_weight(const Gun& g)
{
    double w = g.weight_g;
    for (auto& m : g.mods) w += m.weight_g;
    return w;
}
double effective_volume(const Gun& g)
{
    double v = g.volume_ml;
    for (auto& m : g.mods) v += m.volume_ml;
    return v;
}
