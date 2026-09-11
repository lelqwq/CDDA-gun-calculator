// =============================================================================
//  gun_data.h  —  数据结构、常量、内置数据库的接口
// -----------------------------------------------------------------------------
//  这是"数据层"的头文件：定义长什么样、有哪些字段、能查什么。
//
//  ★ 枪械 / 弹药 / 配件的数据不再手写，而是由脚本从游戏数据生成：
//        python scripts/gen_gun_data.py --game "<游戏目录>"
//    产物在 src/generated/ 下，请勿手工编辑。
//
//  字段名尽量与 Cataclysm-DDA 的 JSON 字段保持一致，方便回查。
// =============================================================================

#pragma once

#include <algorithm>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
//  一、常量
// =============================================================================

constexpr double MAX_RECOIL             = 3000.0;  // game_constants.h:103
constexpr int    MAX_SKILL              = 10;      // game_constants.h:92
constexpr double MIN_RECOIL_IMPROVEMENT = 0.01;    // ranged.h:24
constexpr double GUN_DISPERSION_DIVIDER = 18.0;    // data/core/external_options.json
constexpr int    DISPERSION_PER_GUN_DAMAGE = 30;   // data/core/external_options.json

// 命中档位阈值  game_constants.h:96-100
constexpr double ACC_HEADSHOT = 0.1;
constexpr double ACC_CRITICAL = 0.2;
constexpr double ACC_GOODHIT  = 0.5;
constexpr double ACC_STANDARD = 0.8;
constexpr double ACC_GRAZING  = 1.0;

// =============================================================================
//  二、数据结构
// =============================================================================

// 配件  islot_gunmod  item_factory.cpp:3844
struct GunMod {
    std::string id;
    std::string name;                  // 中文显示名
    std::string name_en;               // 英文原名（搜索用）
    std::string location;              // rail / sights / muzzle / underbarrel / stock ...
                                       // ★ 逻辑键，与游戏 JSON 一致，不要翻译

    double      handling_modifier   = 0.0;   // 越大后坐越小
    double      dispersion_modifier = 0.0;
    double      aim_speed_modifier  = 0.0;   // 越大瞄得越快
    double      sight_dispersion    = -1.0;
    double      field_of_view       = -1.0;
    double      weight_g            = 0.0;   // 装上后给枪增加的重量
    double      volume_ml           = 0.0;   // 装上后给枪增加的体积
    double      barrel_length_mm    = 0.0;   // 上机匣提供的枪管长度
    bool        bipod               = false; // BIPOD flag：只在架设时计入 handling
    bool        laser_sight         = false; // LASER_SIGHT flag：受光照/距离限制
    bool        zoom                = false; // ZOOM flag：视差减到 1/4

    std::vector<std::string> ammo_modifier;  // 上机匣提供的口径（模块化枪械用）
    std::vector<std::string> mod_targets;    // 可装的枪类型或具体枪械 id
    std::vector<std::string> add_mod;        // 装上后解锁的槽位（上机匣提供 rail/sights 等）

    std::string source;                      // "core" 或 mod 名
};

// 弹药  islot_ammo
struct Ammo {
    std::string id;
    std::string name;
    std::string name_en;
    std::string ammo_type;             // 口径，与枪的 ammo_types 对应
    double      recoil     = 0.0;      // ★ DDA 的后坐全部来自弹药
    double      dispersion = 0.0;
    double      range      = 0.0;

    // 散布随枪管长度的修正表 { 枪管长度mm, 修正值 }，递增排列。
    // 对应 JSON 的 "dispersion_modifier"，参与 islot_ammo::dispersion_considering_length
    std::vector<std::pair<double, double>> disp_by_barrel;

    std::string source;
};

// 射击模式  islot_gun::modes  itype.h:892
//
//  游戏 JSON 里长这样： "modes": [ [ "DEFAULT", "semi-auto", 1 ], [ "AUTO", "auto", 4 ] ]
//  数组第 4 个元素是可选的 flag（如 "NPC_AVOID"），本项目用不到，不存。
//
//  ★ DEFAULT / AUTO / BURST 这些 id **没有硬编码语义**，只是约定俗成的名字。
//    半自动和全自动在代码里唯一的差别就是 qty —— 都是同一个 fire_gun 循环，
//    瞄准和后坐算法完全共用（0.I ranged.cpp:1067 的 while( curshot != shots )）。
struct GunMode {
    std::string id;         // 逻辑键：DEFAULT / AUTO / BURST …（不翻译）
    std::string name;       // 游戏里的显示名："semi-auto" / "auto" / "3 rd."
    int         qty = 1;    // 一次扣扳机打几发
};

// 枪械  islot_gun
struct Gun {
    std::string id;
    std::string name;                  // 中文显示名
    std::string name_en;               // 英文原名（搜索用）
    std::string skill;                 // ★ 逻辑键：pistol / rifle / shotgun / smg / launcher / archery

    double      dispersion       = 0.0;   // 枪本身散布（JSON 原值，未除 18）
    double      sight_dispersion = 30.0;  // item_factory.cpp:3411 默认 30
    double      handling         = -1.0;  // <0 表示按类型自动取（步枪/SMG/霰弹枪 20，其余 10）
    double      durability       = 8.0;
    double      recoil           = 0.0;   // DDA 里几乎全是 0
    double      weight_g         = 0.0;
    double      volume_ml        = 0.0;
    double      longest_side_mm  = 0.0;
    double      min_cycle_recoil = 0.0;
    double      barrel_length_mm = 0.0;   // 弹药伤害插值用

    bool        disable_sights = false;    // DISABLE_SIGHTS flag：只能用腰射

    // RELOAD_AND_SHOOT（弓弩、投石索）：打完 recoil 直接回满 MAX_RECOIL，
    // 不参与「连射累积」。0.I ranged.cpp:1225-1227
    bool        reload_and_shoot = false;

    std::vector<GunMode> modes;            // 射击模式，至少一个（DEFAULT）

    std::vector<std::string> ammo_types;   // 可用的口径（模块化枪械为空，口径来自配件）
    std::vector<std::string> mod_slots;    // valid_mod_locations 的槽位名
    std::vector<std::string> aliases;      // 变体的中文名
    std::vector<std::string> aliases_en;   // 变体的英文名

    std::string source;                    // "core" 或 mod 名

    std::vector<GunMod> mods;              // 已安装的配件

    bool has_mod( const std::string &loc ) const {
        for( const auto &m : mods ) if( m.location == loc ) return true;
        return false;
    }
};

// 人物
struct Character {
    std::string gun_skill          = "rifle";
    std::string marksmanship       = "gun";
    double      skill_level        = 0.0;
    double      marksmanship_level = 0.0;
    double      str = 8.0;
    double      dex = 8.0;
    double      per = 8.0;
    // 肢体评分（健康时全为 1.0）
    double      grip   = 1.0;   // 握力
    double      manip  = 1.0;   // 操作
    double      lift   = 1.0;   // 举重
    double      vision = 1.0;   // 视觉

    double skill( const std::string &s ) const {
        return ( s == gun_skill ) ? skill_level
             : ( s == marksmanship ? marksmanship_level : 0.0 );
    }
    // 用于散布的技能 = 枪械技能与枪种技能的均值  ranged.cpp:2698
    double avg_skill( const std::string &gun_s ) const {
        double v = ( marksmanship_level + skill( gun_s ) ) / 2.0;
        return std::min( v, double( MAX_SKILL ) );
    }
};

// =============================================================================
//  三、内置数据库
// =============================================================================

extern std::vector<Gun>    g_guns;
extern std::vector<Ammo>   g_ammo;
extern std::vector<GunMod> g_mods;

// 装填数据库。程序启动时调用一次。
// 内部会依次调用生成的 load_generated_guns / _ammo / _gunmods。
void init_database();

// ---- 供生成的代码调用的填充函数 ---------------------------------------------
// 参数顺序与 src/generated/ 下三个文件里的调用严格对应。
// 改这里的签名，必须同步改 scripts/gen_gun_data.py。

void add_gun( const char *id, const char *name, const char *name_en, const char *skill,
              double dispersion, double sight_dispersion, double handling, double durability,
              double recoil, double weight_g, double volume_ml, double longest_side_mm,
              double min_cycle_recoil, double barrel_length_mm, bool disable_sights,
              std::initializer_list<const char *> ammo_types,
              std::initializer_list<const char *> mod_slots,
              std::initializer_list<const char *> aliases_zh,
              std::initializer_list<const char *> aliases_en,
              std::initializer_list<GunMode> modes,
              bool reload_and_shoot,
              const char *source );

void add_ammo( const char *id, const char *name, const char *name_en, const char *ammo_type,
               double recoil, double dispersion, double range,
               std::initializer_list<std::pair<double, double>> disp_by_barrel,
               const char *source );

void add_gunmod( const char *id, const char *name, const char *name_en, const char *location,
                 double handling_modifier, double dispersion_modifier, double aim_speed_modifier,
                 double sight_dispersion, double field_of_view,
                 double weight_g, double volume_ml, double barrel_length_mm, bool bipod,
                 bool laser_sight, bool zoom,
                 std::initializer_list<const char *> ammo_modifier,
                 std::initializer_list<const char *> mod_targets,
                 std::initializer_list<const char *> add_mod,
                 const char *source );

// 生成的代码（定义在 src/generated/ 下）
void load_generated_guns();
void load_generated_ammo();
void load_generated_gunmods();
