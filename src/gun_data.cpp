// =============================================================================
//  gun_data.cpp  —  内置数据库
// -----------------------------------------------------------------------------
//  ★ 想改枪械/弹药/配件的数值，只动这个文件 ★
//
//  数值取自 Cataclysm-DDA 的 data/json/，注释里标了出处。
//  中文名取自游戏 lang/mo/zh_CN/LC_MESSAGES/cataclysm-dda.mo 的官方译文
//  （提取方法见 extract_zh.py）。
//
//  加一把新枪：在 init_database() 的"枪械"段照抄一段，改字段即可。
//  加一个新配件：在"配件"段加一行 mk_mod(...)。
//  加一种弹药：在"弹药"段加一行。
//
//  注意：GunMod.location / Gun.skill / GunMod.id 是**逻辑键**，
//  必须和代码里的比较、以及游戏 JSON 保持一致，不要翻译成中文。
// =============================================================================

#include "gun_data.h"

std::vector<Gun>    g_guns;
std::vector<Ammo>   g_ammo;
std::vector<GunMod> g_mods;

// 配件工厂：只想设名字/槽位/操控/瞄准时用它，其余字段留默认值
static GunMod mk_mod( const std::string &id, const std::string &name, const std::string &loc,
                      double handling = 0, double aim = 0, double sight = -1, double fov = -1 )
{
    GunMod m;
    m.id = id; m.name = name; m.location = loc;
    m.handling_modifier = handling;
    m.aim_speed_modifier = aim;
    m.sight_dispersion = sight;
    m.field_of_view = fov;
    return m;
}

void init_database()
{
    // =========================================================================
    //  弹药
    // =========================================================================
    // 注意：这里只填本程序真正用到的字段（recoil / dispersion）。
    // damage / armor_pen / loudness 未经核对，故不预置 —— 需要的话请自行从
    // data/json/items/ammo/*.json 抄入，并在 Ammo 结构里接进计算。
    { Ammo a; a.id="10mm_fmj"; a.name="10mm FMJ 弹";  a.recoil=750;  a.dispersion=50; a.range=14; g_ammo.push_back(a); }
    { Ammo a; a.id="223_rem";  a.name="5.56x45mm 弹"; a.recoil=1350; a.dispersion=45; a.range=16; g_ammo.push_back(a); }
    { Ammo a; a.id="bp_10mm";  a.name="10mm 黑火药弹"; a.recoil=570;  a.dispersion=60; a.range=12; g_ammo.push_back(a); }

    // =========================================================================
    //  配件：瞄准速度类（data/json/items/gunmod/）
    // =========================================================================
    g_mods.push_back(mk_mod("laser_sight",   "管下激光瞄具",         "underbarrel", 0, 15, 30, 3000));
    g_mods.push_back(mk_mod("rail_laser",    "导轨激光瞄具",         "rail",        0, 15, 30, 3000));
    g_mods.push_back(mk_mod("mipim",         "军用战术手电激光模块", "rail",        0, 15, 30, 3000));
    g_mods.push_back(mk_mod("red_dot_sight", "红点瞄准镜",           "sights",      0, 10, 27,  630));
    g_mods.push_back(mk_mod("holo_sight",    "全息瞄准镜",           "sights",      0, 10, 23,  720));
    g_mods.push_back(mk_mod("holo_magnifier","瞄准镜变焦器",         "magnifier",   0,  5, 13,  270));
    g_mods.push_back(mk_mod("rifle_scope",   "步枪瞄准镜",           "sights",      0, -1, 10,  180));

    // =========================================================================
    //  配件：后坐 / 操控类
    // =========================================================================
    { GunMod m = mk_mod("bipod","两脚架","underbarrel",18); m.bipod = true; g_mods.push_back(m); }
    g_mods.push_back(mk_mod("modern_handguard",   "高端护木",         "underbarrel", 6));
    g_mods.push_back(mk_mod("grip",               "前置握把",         "underbarrel", 6));
    g_mods.push_back(mk_mod("offset_grip",        "侧面握把",         "rail",        4));
    g_mods.push_back(mk_mod("recoil_stock",       "后坐缓冲枪托",     "stock",       4));
    g_mods.push_back(mk_mod("muzzle_brake",       "枪口制退器",       "muzzle",      4));
    g_mods.push_back(mk_mod("barrel_ported",      "气孔式枪管",       "barrel",      4));
    g_mods.push_back(mk_mod("cheek_pad",          "托腮板",           "stock accessory", 2));
    g_mods.push_back(mk_mod("compensator",        "补偿器",           "muzzle",      1));
    g_mods.push_back(mk_mod("adjustable_stock",   "可调节枪托",       "stock",       1));
    g_mods.push_back(mk_mod("folding_stock_folded","折叠枪托（折叠）", "stock",     -15));

    // =========================================================================
    //  枪械
    // =========================================================================
    {   // M4A1 = modular_m4_carbine 的变体 + retool_ar15_223rem 上机匣
        Gun gun;
        gun.id   = "m4a1";
        gun.name = "M4A1 卡宾枪（含 .223 中长上机匣）";
        gun.skill            = "rifle";
        gun.dispersion       = 180;              // 223.json
        gun.sight_dispersion = 40;               // gun_base_rifle_semi
        gun.handling         = 20;               // item_factory.cpp:761 步枪默认
        gun.durability       = 8;
        gun.recoil           = 0;
        gun.weight_g         = 880 + 2268;       // 下机匣 + 上机匣
        gun.volume_ml        = 1760 + 1550;
        gun.longest_side_mm  = 850;              // 组装后全长（近似）
        gun.min_cycle_recoil = 1350;
        g_guns.push_back(gun);
    }
    {   // Glock 29
        Gun gun;
        gun.id   = "glock_29";
        gun.name = "格洛克 29 手枪（10mm）";
        gun.skill            = "pistol";
        gun.dispersion       = 510;              // 10mm.json
        gun.sight_dispersion = 60;               // gun_base_handgun_semi
        gun.handling         = 10;               // 手枪默认
        gun.durability       = 8;
        gun.weight_g         = 690;
        gun.volume_ml        = 410;
        gun.longest_side_mm  = 177;
        gun.min_cycle_recoil = 720;
        g_guns.push_back(gun);
    }
}
