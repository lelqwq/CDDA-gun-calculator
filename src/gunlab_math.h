// =============================================================================
//  gunlab_math.h  —  纯计算逻辑的接口（gunlab_core 的一部分）
// -----------------------------------------------------------------------------
//  全部是纯函数：输入枪 / 弹药 / 配件 / 人物，输出数值，不做任何输入输出。
//  命令行版（gunlab.cpp）与图形版（src/gui/）共用这一层。
//
//  ★ 改公式前先在 ../Cataclysm-DDA 里确认 0.I 的写法：
//      git show 27939e29b8b4ddc081490d9f51de59a459c88df6:src/item.cpp
//    本项目对齐的是 0.I 稳定版，而源码仓库是 master（0.J 开发中），
//    两者显示格式已经不同。详见 CLAUDE.md。
// =============================================================================

#pragma once

#include <random>
#include <string>
#include <utility>
#include <vector>

#include "gun_data.h"
#include "zh_cn.h"

// ---- 瞄准上下文与结果 --------------------------------------------------------

// 一次瞄准模拟的输入参数
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

// 瞄准模拟的结果
struct AimResult {
    int    moves_to_regular = -1;
    int    moves_to_careful = -1;
    int    moves_to_precise = -1;
    double regular_th = 0, careful_th = 0, precise_th = 0;
    double recoil_after_1_turn = 0;
    double delta_1_turn        = 0;
};

// ---- 公式 --------------------------------------------------------------------

double logarithmic(double t);
double logarithmic_range(int mn, int mx, int pos);
int ranged_per_mod(double per);
int get_character_parallax(double per, double vision, bool zoom);
double effective_dispersion(double per, double vision, double disp, bool zoom);
double point_shooting_limit(double skill, bool archery);
double modified_sight_speed(double aim_speed_modifier, double eff_sight_disp, double recoil);
double most_accurate_aiming_method_limit(const Gun& g, const Character& c);
double fastest_aiming_method_speed(const Gun& g, const Character& c, double recoil, double target_range, double target_size_moa, bool visible);
double aim_factor_from_volume(const Gun& g, double volume_ml);
double aim_factor_from_length(double length_mm, bool enclosed);
double aim_per_move(const Gun& g, const Character& c, double recoil, const AimContext& ctx);
double gun_base_weight(const Gun& g);
int gun_recoil(const Gun& g, double arm_str, double ammo_recoil, bool bipod = false, bool ideal_strength = false);
double added_recoil_per_shot(int qty, double absorb);
double recoil_absorb(double skill);
double game_dispersion_gun(const Gun& g);
double game_dispersion_ammo(const Gun& g, const Ammo* ammo);
std::pair<int, int> sight_dispersion_pair(const Gun& g, const Character& c);
double game_recoil(const Gun& g, const Character& c, const Ammo* ammo);
double game_recoil_bipod(const Gun& g, const Character& c, const Ammo* ammo);
double game_min_recoil(const Gun& g, const Character& c, const Ammo* ammo);
double multi_lerp(const std::vector<std::pair<double, double>>& points, double x);
double effective_barrel_length(const Gun& g);
double dispersions_considering_length(const Ammo& ammo, double barrel_length_mm);
double gun_dispersion(const Gun& g, const Ammo* ammo, int damage_level = 0, bool with_scaling = true);
double dispersion_from_skill(double skill, double weapon_dispersion);
double get_weapon_dispersion(const Gun& g, const Character& c, const Ammo* ammo);
int range_with_even_chance_of_good_hit(double dispersion);
double iso_tangent(double distance_tiles, double angle_arcmin);
double missed_by(double total_dispersion, double range_tiles, double target_size);
double roll_dispersion(const Gun& g, const Character& c, const Ammo* ammo, double recoil, std::mt19937& rng);
int tier_index(double missed_by);
AimResult simulate_aim(const Gun& g, const Character& c, AimContext ctx, int turn_moves = 100, int max_moves = 5000);
bool icontains(const std::string& hay, const std::string& needle);
std::vector<std::string> gun_search_keys(const Gun& g);
std::vector<int> search_guns(const std::string& kw);
std::vector<std::string> effective_ammo_types(const Gun& g);
std::vector<int> ammo_for_gun(const Gun& g);
const Ammo* pick_default_ammo(const Gun& g);
std::string gun_type_of(const Gun& g);
std::vector<std::string> available_slots(const Gun& g);
bool mod_fits_gun(const Gun& g, const GunMod& m);
double effective_weight(const Gun& g);
double effective_volume(const Gun& g);
