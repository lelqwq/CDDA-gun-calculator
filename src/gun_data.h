// =============================================================================
//  gun_data.h  —  数据结构、常量、内置数据库的接口
// -----------------------------------------------------------------------------
//  这是"数据层"的头文件：定义长什么样、有哪些字段、能查什么。
//  实际数值在 gun_data.cpp 里 —— 想改枪械/弹药/配件数据，只动那个文件。
//
//  字段名尽量与 Cataclysm-DDA 的 JSON 字段保持一致，方便回查：
//    data/json/items/gun/*.json
//    data/json/items/ammo/*.json
//    data/json/items/gunmod/*.json
//    src/itype.h（islot_gun / islot_gunmod / islot_ammo）
// =============================================================================

#pragma once

#include <algorithm>
#include <string>
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
    std::string name;                  // 显示名（中文）
    std::string location;              // rail / sights / muzzle / underbarrel / stock ...
                                       // 注意：这是**逻辑键**，代码里有 has_mod() 比较，
                                       // 且与游戏 JSON 的 location 字段一致，不要改成中文
    double      handling_modifier   = 0.0;   // 越大后坐越小
    double      dispersion_modifier = 0.0;   // 加到枪基础散布
    double      aim_speed_modifier  = 0.0;   // 越大瞄得越快
    double      sight_dispersion    = -1.0;
    double      field_of_view       = -1.0;
    bool        bipod               = false; // 带 BIPOD flag：只在架设时计入 handling
    bool        laser_sight         = false;
    bool        zoom                = false;
    double      weight_g            = 0.0;
    double      volume_ml           = 0.0;
    double      longest_side_mm     = 0.0;
    double      range_modifier      = 0.0;
};

// 弹药  islot_ammo
struct Ammo {
    std::string id;
    std::string name;
    double      recoil     = 0.0;      // 本程序真正用到的字段
    double      dispersion = 0.0;      // 本程序真正用到的字段
    double      range      = 0.0;
    double      loudness   = 0.0;      // 以下字段当前未参与计算，需要时自行接进公式
    std::string damage_type = "bullet";
    double      damage     = 0.0;
    double      armor_pen  = 0.0;
};

// 枪械  islot_gun
struct Gun {
    std::string id;
    std::string name;
    std::string skill        = "rifle";   // pistol / rifle / shotgun / smg / launcher / archery
                                          // 同样是与游戏一致的逻辑键，不要改中文
    double      dispersion   = 0.0;       // 枪本身散布（JSON 原值，未除 18）
    double      sight_dispersion = 40.0;  // 铁瞄散布
    double      handling     = 10.0;      // 操控性；<0 表示按类型自动取（步枪 20 / 其他 10）
    double      durability   = 8.0;
    double      recoil       = 0.0;       // 枪本身后坐（DDA 里几乎全是 0，后坐来自弹药）
    double      weight_g     = 0.0;
    double      volume_ml    = 0.0;
    double      longest_side_mm = 0.0;
    double      min_cycle_recoil = 0.0;
    bool        ammo_required    = true;
    bool        disable_sights   = false;  // DISABLE_SIGHTS flag
    bool        primitive_ranged = false;  // PRIMITIVE_RANGED_WEAPON flag

    std::vector<GunMod> mods;

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
//  三、内置数据库（定义在 gun_data.cpp）
// =============================================================================

extern std::vector<Gun>    g_guns;
extern std::vector<Ammo>   g_ammo;
extern std::vector<GunMod> g_mods;

// 装填上面的数据库。程序启动时调用一次。
void init_database();
