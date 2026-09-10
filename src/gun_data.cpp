// =============================================================================
//  gun_data.cpp  —  数据库容器与填充函数
// -----------------------------------------------------------------------------
//  实际数据不在这里，而是由脚本从游戏数据生成，位于 src/generated/：
//      python scripts/gen_gun_data.py --game "<游戏目录>"
//
//  本文件只负责：
//    · 定义三个全局容器
//    · 提供生成的代码调用的 add_gun / add_ammo / add_gunmod
//    · init_database() 把生成的三个加载函数串起来
//
//  想手写几条测试数据？直接在 init_database() 里调 add_gun(...) 即可，
//  生成的数据会先装填，你的手写条目追加在后面。
// =============================================================================

#include "gun_data.h"

std::vector<Gun>    g_guns;
std::vector<Ammo>   g_ammo;
std::vector<GunMod> g_mods;

// -----------------------------------------------------------------------------
//  填充函数（供 src/generated/ 下的代码调用）
// -----------------------------------------------------------------------------

void add_gun( const char *id, const char *name, const char *name_en, const char *skill,
              double dispersion, double sight_dispersion, double handling, double durability,
              double recoil, double weight_g, double volume_ml, double longest_side_mm,
              double min_cycle_recoil, double barrel_length_mm, bool disable_sights,
              std::initializer_list<const char *> ammo_types,
              std::initializer_list<const char *> mod_slots,
              std::initializer_list<const char *> aliases_zh,
              std::initializer_list<const char *> aliases_en,
              const char *source )
{
    Gun g;
    g.id   = id ? id : "";
    g.name = name ? name : "";
    g.name_en = name_en ? name_en : "";
    g.skill   = skill ? skill : "";

    g.dispersion       = dispersion;
    g.sight_dispersion = sight_dispersion;
    g.durability       = durability;
    g.recoil           = recoil;
    g.weight_g         = weight_g;
    g.volume_ml        = volume_ml;
    g.longest_side_mm  = longest_side_mm;
    g.min_cycle_recoil = min_cycle_recoil;
    g.barrel_length_mm = barrel_length_mm;
    g.disable_sights   = disable_sights;
    g.source           = source ? source : "core";

    // handling < 0 表示"按类型自动取"  item_factory.cpp:761
    if( handling < 0 ) {
        const bool heavy = ( g.skill == "rifle" || g.skill == "smg" || g.skill == "shotgun" );
        g.handling = heavy ? 20.0 : 10.0;
    } else {
        g.handling = handling;
    }

    for( const char *s : ammo_types ) g.ammo_types.push_back( s ? s : "" );
    for( const char *s : mod_slots  ) g.mod_slots.push_back( s ? s : "" );
    for( const char *s : aliases_zh ) g.aliases.push_back( s ? s : "" );
    for( const char *s : aliases_en ) g.aliases_en.push_back( s ? s : "" );

    g_guns.push_back( g );
}

void add_ammo( const char *id, const char *name, const char *name_en, const char *ammo_type,
               double recoil, double dispersion, double range, const char *source )
{
    Ammo a;
    a.id   = id ? id : "";
    a.name = name ? name : "";
    a.name_en   = name_en ? name_en : "";
    a.ammo_type = ammo_type ? ammo_type : "";
    a.recoil     = recoil;
    a.dispersion = dispersion;
    a.range      = range;
    a.source     = source ? source : "core";
    g_ammo.push_back( a );
}

void add_gunmod( const char *id, const char *name, const char *name_en, const char *location,
                 double handling_modifier, double dispersion_modifier, double aim_speed_modifier,
                 double sight_dispersion, double field_of_view,
                 double weight_g, double volume_ml, bool bipod,
                 bool laser_sight, bool zoom,
                 std::initializer_list<const char *> ammo_modifier,
                 std::initializer_list<const char *> mod_targets,
                 std::initializer_list<const char *> add_mod,
                 const char *source )
{
    GunMod m;
    m.id   = id ? id : "";
    m.name = name ? name : "";
    m.name_en  = name_en ? name_en : "";
    m.location = location ? location : "";

    m.handling_modifier   = handling_modifier;
    m.dispersion_modifier = dispersion_modifier;
    m.aim_speed_modifier  = aim_speed_modifier;
    m.sight_dispersion    = sight_dispersion;
    m.field_of_view       = field_of_view;
    m.weight_g            = weight_g;
    m.volume_ml           = volume_ml;
    m.bipod               = bipod;
    m.laser_sight         = laser_sight;
    m.zoom                = zoom;
    m.source              = source ? source : "core";

    for( const char *s : ammo_modifier ) m.ammo_modifier.push_back( s ? s : "" );
    for( const char *s : mod_targets  ) m.mod_targets.push_back( s ? s : "" );
    for( const char *s : add_mod      ) m.add_mod.push_back( s ? s : "" );

    g_mods.push_back( m );
}

// -----------------------------------------------------------------------------
//  初始化
// -----------------------------------------------------------------------------

void init_database()
{
    // 由游戏数据生成，见 src/generated/
    load_generated_guns();
    load_generated_ammo();
    load_generated_gunmods();
}
