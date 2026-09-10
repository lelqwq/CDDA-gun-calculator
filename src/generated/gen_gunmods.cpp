// =============================================================================
//  gen_gunmods.cpp  生成的配件数据  —  自动生成，请勿手工编辑
// -----------------------------------------------------------------------------
//  由 scripts/gen_gun_data.py 从 Cataclysm-DDA 的游戏数据生成。
//  重新生成：
//      python scripts/gen_gun_data.py --game "<游戏目录>"
//
//  数据来源：core
//  条目数：170
//
//  ★ 字段顺序与 gun_data.h 里的结构体严格对应，改结构体必须同步改生成器 ★
// =============================================================================

#include "gun_data.h"

void load_generated_gunmods()
{
    auto &V = g_mods;
    V.reserve(170);

    // 字段顺序：id, name, name_en, location, handling_modifier, dispersion_modifier, aim_speed_modifier, sight_dispersion, field_of_view, weight_g, volume_ml, bipod, laser_sight, zoom, ammo_modifier, mod_targets, source

    add_gunmod("acog_scope", "ACOG 瞄准镜", "ACOG scope", "sights", 0, 0, 0, 8, 270, 280, 112, false, false, true, {}, {"rifle", "crossbow", "launcher", "shotgun", "smg"}, "core");
    add_gunmod("adjustable_stock", "可调节枪托", "adjustable stock", "stock", 1, -1, 0, -1, -1, 350, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("arisaka_bayonet", "30式刺刀", "Type 30 bayonet", "bayonet lug", 0, 0, 0, -1, -1, 700, 380, false, false, false, {}, {"type99", "type_99_sniper"}, "core");
    add_gunmod("arisaka_monopod", "铁丝独脚架（折叠）", "wire monopod, collapsed", "underbarrel", 0, 0, 0, -1, -1, 120, 250, false, false, false, {}, {"type99"}, "core");
    add_gunmod("arisaka_monopod_deployed", "铁丝独脚架（展开）", "wire monopod, deployed", "underbarrel", 6, 0, 0, -1, -1, 120, 250, true, false, false, {}, {"type99"}, "core");
    add_gunmod("arredondo_chute", "快速装弹器滑槽", "speedloader chute", "loading port", 0, 0, 0, -1, -1, 135, 149, false, false, false, {}, {"pistol", "shotgun"}, "core");
    add_gunmod("arredondo_chute_benelli_sa", "快速装弹器滑槽（4发水禽霰弹枪）", "speedloader chute, 4-round waterfowl shotgun", "loading port", 0, 0, 0, -1, -1, 135, 149, false, false, false, {}, {"benelli_sa"}, "core");
    add_gunmod("arredondo_chute_mossberg_500", "快速装弹器滑槽（6发战斗霰弹枪）", "speedloader chute, 6-round combat shotgun", "loading port", 0, 0, 0, -1, -1, 135, 149, false, false, false, {}, {"mossberg_500"}, "core");
    add_gunmod("arredondo_chute_mossberg_590", "快速装弹器滑槽（9发战斗霰弹枪）", "speedloader chute, 9-round combat shotgun", "loading port", 0, 0, 0, -1, -1, 135, 149, false, false, false, {}, {"mossberg_590"}, "core");
    add_gunmod("arredondo_chute_mossberg_930", "快速装弹器滑槽（8发自动霰弹枪）", "speedloader chute, 8-round auto-loading shotgun", "loading port", 0, 0, 0, -1, -1, 135, 149, false, false, false, {}, {"mossberg_930"}, "core");
    add_gunmod("arredondo_chute_remington_870", "快速装弹器滑槽（5发狩猎霰弹枪）", "speedloader chute, 5-round hunting shotgun", "loading port", 0, 0, 0, -1, -1, 135, 149, false, false, false, {}, {"remington_870"}, "core");
    add_gunmod("arredondo_chute_remington_870_breacher", "快速装弹器滑槽（4发破门霰弹枪）", "speedloader chute, 4-round breaching shotgun", "loading port", 0, 0, 0, -1, -1, 135, 149, false, false, false, {}, {"remington_870_breacher"}, "core");
    add_gunmod("arrowrest", "箭托", "arrow rest", "arrow rest", 0, -20, 0, -1, -1, 16, 250, false, false, false, {}, {"bow"}, "core");
    add_gunmod("arrowrest_wood", "木制箭托", "wooden arrow rest", "arrow rest", 0, -20, 0, -1, -1, 100, 250, false, false, false, {}, {"bow"}, "core");
    add_gunmod("barrel_ported", "气孔式枪管", "ported barrel", "barrel", 4, 15, 0, -1, -1, 225, 200, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun"}, "core");
    add_gunmod("barrel_small", "锯短枪管", "shortened barrel", "barrel", 0, 75, 0, -1, -1, 0, 0, false, false, false, {}, {"smg", "rifle", "shotgun"}, "core");
    add_gunmod("bars", "后坐力平衡系统", "balanced recoil system", "mechanism", 15, 0, 0, -1, -1, 60, 250, false, false, false, {}, {"kord"}, "core");
    add_gunmod("beam_scatterer", "激光散射器", "beam scatterer", "lens", 0, 0, 0, -1, -1, 380, 250, false, false, false, {}, {"rifle", "pistol"}, "core");
    add_gunmod("belt_clip", "皮带扣", "belt clip", "belt clip", 0, 0, 0, -1, -1, 40, 250, false, false, false, {}, {"rugerlcp", "kp32"}, "core");
    add_gunmod("big_brass_catcher", "弹壳收集器（大）", "big brass catcher", "brass catcher", 0, 0, 0, -1, -1, 228, 300, false, false, false, {}, {"shotgun", "smg", "rifle", "pistol", "launcher"}, "core");
    add_gunmod("bipod", "两脚架", "bipod", "underbarrel", 18, 0, 0, -1, -1, 400, 500, true, false, false, {}, {"rifle", "launcher", "smg", "shotgun", "crossbow"}, "core");
    add_gunmod("bipod_handguard", "护木型两脚架", "bipod hand guard", "underbarrel", 4, 0, 0, -1, -1, 400, 500, false, false, false, {}, {"rifle"}, "core");
    add_gunmod("bipod_handguard_deployed", "护木型两脚架（展开）", "bipod hand guard (deployed)", "underbarrel", 18, 0, 0, -1, -1, 400, 500, true, false, false, {}, {"rifle"}, "core");
    add_gunmod("bow_scope", "弓用瞄准镜", "bow scope", "sights", 0, 0, 0, 10, 270, 180, 250, false, false, true, {}, {"bow"}, "core");
    add_gunmod("bow_sight", "五针弓瞄", "five pin bow sight", "sights", 0, 0, 8, 23, 900, 240, 553, false, false, false, {}, {"bow"}, "core");
    add_gunmod("bow_sight_pin", "单针弓瞄", "single pin bow sight", "sights", 0, 0, 8, 30, 900, 80, 50, false, false, false, {}, {"bow"}, "core");
    add_gunmod("bow_silencer", "弓箭减震器", "bow dampening kit", "dampening", 1, -2, 0, -1, -1, 50, 250, false, false, false, {}, {"bow", "crossbow", "rifle", "smg", "pistol"}, "core");
    add_gunmod("bow_stabilizer", "弓箭稳定器（前杆）", "bow stabilizer", "stabilizer", 3, -100, 0, -1, -1, 88, 500, false, false, false, {}, {"bow"}, "core");
    add_gunmod("bow_stabilizer_set", "弓箭稳定器（全套）", "bow stabilizer system", "stabilizer", 6, -200, 0, -1, -1, 300, 500, false, false, false, {}, {"bow"}, "core");
    add_gunmod("brass_catcher", "弹壳收集器", "brass catcher", "brass catcher", 0, 0, 0, -1, -1, 114, 250, false, false, false, {}, {"shotgun", "smg", "rifle", "pistol", "launcher"}, "core");
    add_gunmod("butt_hook", "枪托尾钩", "butt hook", "stock accessory", 0, -15, 0, -1, -1, 100, 100, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("cheek_pad", "托腮板", "cheek pad", "stock accessory", 2, -1, 0, -1, -1, 300, 250, false, false, false, {}, {"rifle"}, "core");
    add_gunmod("choke", "喉缩", "choke", "muzzle", 0, 0, 0, -1, -1, 300, 150, false, false, false, {}, {"shotgun"}, "core");
    add_gunmod("compensator", "补偿器", "compensator", "muzzle", 1, 0, 0, -1, -1, 82, 17, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun"}, "core");
    add_gunmod("condenser_pristine", "崭新的冷凝器", "pristine condenser", "condenser", 0, 0, 0, -1, -1, 380, 250, false, false, false, {}, {"launcher", "rifle"}, "core");
    add_gunmod("crafted_suppressor", "自制消音器", "homemade suppressor", "muzzle", 1, 40, 0, -1, -1, 880, 750, false, false, false, {}, {"pistol", "smg", "rifle"}, "core");
    add_gunmod("dias", "无损式自动阻铁", "drop-in auto sear", "mechanism", 0, 10, 0, -1, -1, 113, 250, false, false, false, {}, {"modular_ar15", "ar_pistol", "modular_ar_pistol"}, "core");
    add_gunmod("effective_emitter", "高效发射器", "effective emitter", "emitter", 0, 0, 0, -1, -1, 380, 250, false, false, false, {}, {"pistol", "rifle"}, "core");
    add_gunmod("electrolaser_conversion", "电子激光束转换器", "electrolaser conversion", "lens", 0, 0, 0, -1, -1, 380, 1000, false, false, false, {}, {"pistol"}, "core");
    add_gunmod("enfield_bayonet", "恩菲尔德四号步枪刺刀", "Enfield No. 4 bayonet", "bayonet lug", 0, 0, 0, -1, -1, 500, 300, false, false, false, {}, {"number4_mki"}, "core");
    add_gunmod("filter_suppressor", "\"燃油滤清\"消音器", "'solvent trap' suppressor", "muzzle", 1, 10, 0, -1, -1, 758, 1254, false, false, false, {}, {"pistol", "smg", "rifle"}, "core");
    add_gunmod("focusing_lens", "聚焦透镜", "focusing lens", "lens", 0, 15, 0, -1, -1, 380, 250, false, false, false, {}, {"rifle", "pistol"}, "core");
    add_gunmod("folding_stock", "折叠枪托", "folding stock", "stock", 0, 0, 0, -1, -1, 200, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher"}, "core");
    add_gunmod("folding_stock_folded", "折叠枪托（折叠）", "folding stock (folded)", "stock", -15, 0, 0, -1, -1, 200, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher"}, "core");
    add_gunmod("glocksear", "格洛克全自动挡板阻铁", "Glock auto sear plate", "mechanism", 0, 0, 0, -1, -1, 55, 20, false, false, false, {}, {"glock_19", "glock_17", "glock_22", "glock_31", "glock_21", "glock_20", "glock_29", "glock_40"}, "core");
    add_gunmod("grip", "前置握把", "forward grip", "underbarrel", 6, 0, 0, -1, -1, 68, 119, false, false, false, {}, {"shotgun", "smg", "rifle", "crossbow", "launcher"}, "core");
    add_gunmod("grip_mod", "前置握把（适配改装）", "modified forward grip", "underbarrel", 6, 0, 0, -1, -1, 68, 119, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("grip_mount", "可更换枪具套装", "replaceable furniture kit", "grip mount", 0, 0, 0, -1, -1, 80, 50, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("high_density_capacitor", "高密度电容", "high density capacitor", "emitter", 0, 0, 0, -1, -1, 380, 250, false, false, false, {}, {"pistol", "rifle"}, "core");
    add_gunmod("high_end_folding_stock", "可调式折叠枪托", "adjustable folding stock", "stock", 1, -1, 0, -1, -1, 200, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher"}, "core");
    add_gunmod("high_end_folding_stock_folded", "模块化折叠枪托（折叠）", "modular folding stock (folded)", "stock", -15, 0, 0, -1, -1, 200, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher"}, "core");
    add_gunmod("holo_magnifier", "瞄准镜变焦器", "sight magnifier", "magnifier", 0, 0, 5, 13, 270, 320, 390, false, false, true, {}, {"smg", "rifle", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("holo_sight", "全息瞄准镜", "holographic sight", "sights", 0, 0, 10, 23, 720, 255, 290, false, false, false, {}, {"smg", "rifle", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("hybrid_sight_4x", "混合式 ACOG 瞄准镜", "hybrid ACOG scope", "sights", 0, 0, 0, 8, 270, 280, 112, false, false, true, {}, {"rifle", "crossbow", "launcher", "shotgun", "smg"}, "core");
    add_gunmod("improve_sights", "机械瞄具", "iron sights", "sights", 0, 0, 0, 30, 360, 60, 10, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("inter_bayonet", "集成刺刀", "integrated bayonet", "underbarrel", 0, 0, 0, -1, -1, 1, 92, false, false, false, {}, {"shotgun", "rifle"}, "core");
    add_gunmod("inter_bayonet_folded", "集成刺刀（折叠）", "integrated bayonet (folded)", "underbarrel", 0, 0, 0, -1, -1, 1, 92, false, false, false, {}, {"shotgun", "rifle"}, "core");
    add_gunmod("knife_combat", "国民警卫队刺刀", "National Guard bayonet", "bayonet lug", 0, 0, 0, -1, -1, 270, 184, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("knife_combat_army", "刺刀", "Bayonet", "bayonet lug", 0, 0, 0, -1, -1, 450, 209, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("knife_combat_marine", "海军陆战队刺刀", "USMC bayonet", "bayonet lug", 0, 0, 0, -1, -1, 405, 260, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("laser_sight", "管下激光瞄具", "underbarrel laser sight", "underbarrel", 0, 0, 15, 30, 3000, 70, 40, false, true, false, {}, {"smg", "rifle", "pistol", "shotgun", "crossbow", "bow", "slingshot", "launcher"}, "core");
    add_gunmod("llink", "快装联片阻铁", "lightning link", "mechanism", 0, 40, 0, -1, -1, 60, 250, false, false, false, {}, {"modular_ar15", "ar_pistol", "modular_ar_pistol"}, "core");
    add_gunmod("m203_mod", "M203 榴弹发射器（适配改装）", "modified M203", "underbarrel", 0, 0, 0, -1, -1, 1360, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("m240semi", "M240快慢机", "M240 select fire trigger", "mechanism", 0, 5, 0, -1, -1, 60, 250, false, false, false, {}, {"m240"}, "core");
    add_gunmod("m249semi", "M249快慢机", "M249 select fire trigger", "mechanism", 0, 5, 0, -1, -1, 60, 250, false, false, false, {}, {"m249"}, "core");
    add_gunmod("m26_mass_mod", "M26-MASS 霰弹系统（适配改装）", "modified M26-MASS", "underbarrel", 0, 0, 0, -1, -1, 1220, 1068, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("m26_mass_stock", "MASS 兼容枪托", "MASS compatible stock", "stock", 4, 0, 0, -1, -1, 960, 500, false, false, false, {}, {"m26_mass_standalone"}, "core");
    add_gunmod("m320_mod_mod", "M320 榴弹发射器模组（适配改装）", "modified M320 GLM", "underbarrel", 0, 0, 0, -1, -1, 1500, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("makeshift_bayonet", "简易刺刀", "makeshift bayonet", "underbarrel", 0, 0, 0, -1, -1, 550, 250, false, false, false, {}, {"shotgun", "rifle", "smg", "launcher", "crossbow"}, "core");
    add_gunmod("masterkey_mod", "\"万能钥匙\"管下霰弹枪（适配改装）", "modified masterkey shotgun", "underbarrel", 0, 0, 0, -1, -1, 2600, 1068, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("match_trigger", "竞赛级扳机", "match trigger", "mechanism", 0, -1, 0, -1, -1, 120, 250, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun", "launcher"}, "core");
    add_gunmod("modern_handguard", "高端护木", "high end hand guard", "underbarrel", 6, -5, 0, -1, -1, 220, 500, false, false, false, {}, {"rifle"}, "core");
    add_gunmod("mp5sd_suppressor", "MP5SD 内置消音器", "MP5SD integral suppressor", "barrel", 2, 0, 0, -1, -1, 250, 247, false, false, false, {}, {"pistol", "smg", "rifle"}, "core");
    add_gunmod("muzzle_brake", "枪口制退器", "muzzle brake", "muzzle", 4, 15, 0, -1, -1, 380, 250, false, false, false, {}, {"rifle", "shotgun", "pistol", "smg"}, "core");
    add_gunmod("muzzle_weight", "枪口抑制配重", "muzzle weight", "underbarrel", 2, 0, 0, -1, -1, 179, 68, false, false, false, {}, {"smg", "pistol"}, "core");
    add_gunmod("offset_grip", "侧面握把", "offset grip", "rail", 4, 0, 0, -1, -1, 125, 250, false, false, false, {}, {"shotgun", "smg", "rifle"}, "core");
    add_gunmod("offset_sight_rail", "侧面瞄具导轨", "offset sight rail", "rail", 0, 0, 0, -1, -1, 40, 125, false, false, false, {}, {"shotgun", "smg", "rifle", "launcher"}, "core");
    add_gunmod("offset_sights", "侧面机械瞄具", "offset iron sights", "rail", 0, 0, 0, 37.5, 360, 60, 10, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun", "launcher"}, "core");
    add_gunmod("pistol_bayonet", "手枪刺刀", "pistol bayonet", "underbarrel", 0, 0, 0, -1, -1, 122, 250, false, false, false, {}, {"pistol"}, "core");
    add_gunmod("pistol_scope", "手枪瞄准镜", "pistol scope", "sights", 0, 0, 0, 8, 360, 212, 200, false, false, false, {}, {"pistol", "smg"}, "core");
    add_gunmod("pistol_stock", "手枪枪托", "pistol stock", "stock", 6, 0, 0, -1, -1, 350, 500, false, false, false, {}, {"pistol"}, "core");
    add_gunmod("primitive_bow_silencer", "原始弓箭减震器", "primitive bow dampening kit", "dampening", 1, -2, 0, -1, -1, 50, 250, false, false, false, {}, {"bow", "crossbow", "rifle", "smg", "pistol"}, "core");
    add_gunmod("rail_bayonet_lug", "导轨刺刀底座", "rail bayonet mount", "rail", 0, 0, 0, -1, -1, 162, 40, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("rail_laser_sight", "导轨激光瞄具", "rail laser sight", "rail", 0, 0, 15, 30, 3000, 120, 250, false, true, false, {}, {"smg", "rifle", "pistol", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("rail_mount", "导轨底座", "side mount", "rail mount", 0, 0, 0, -1, -1, 10, 8, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("recoil_stock", "后坐缓冲枪托", "recoil stock", "stock", 4, 0, 0, -1, -1, 960, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher"}, "core");
    add_gunmod("red_dot_sight", "红点瞄准镜", "red dot sight", "sights", 0, 0, 10, 27, 630, 150, 80, false, false, false, {}, {"smg", "rifle", "shotgun", "pistol", "crossbow", "launcher"}, "core");
    add_gunmod("ree_33_tripod", "重型三脚架", "heavy-duty tripod", "underbarrel", 18, 0, 0, -1, -1, 6000, 5800, true, false, false, {}, {"ree_33"}, "core");
    add_gunmod("retool_ar15_22", ".22 上机匣", ".22 upper receiver", "bore", 0, 15, 0, -1, -1, 2275, 1696, false, false, false, {"22"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_223rem", ".223 中长上机匣", "mid-length .223 upper receiver", "bore", 0, 0, 0, -1, -1, 2268, 1550, false, false, false, {"223"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_223rem_extended", ".223 长管上机匣", "rifle .223 upper receiver", "bore", 0, 0, 0, -1, -1, 2408, 1860, false, false, false, {"223"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_223rem_medium", ".223 卡宾上机匣", "carbine .223 upper receiver", "bore", 0, 0, 0, -1, -1, 2036, 1395, false, false, false, {"223"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_223rem_pistol", ".223 手枪上机匣", "pistol .223 upper receiver", "bore", 0, 0, 0, -1, -1, 1134, 718, false, false, false, {"223"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_223rem_short", ".223 CQB上机匣", "CQB .223 upper receiver", "bore", 0, 0, 0, -1, -1, 1819, 1295, false, false, false, {"223"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_300blk", ".300 BLK 中长上机匣", "mid-length .300BLK upper receiver", "bore", 0, 0, 0, -1, -1, 2308, 1550, false, false, false, {"300blk"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_300blk_medium", ".300 BLK CQB上机匣", "CQB .300BLK upper receiver", "bore", 0, 0, 0, -1, -1, 1814, 1295, false, false, false, {"300blk"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_300blk_short", ".300 BLK 短管上机匣", "SBR .300BLK upper receiver", "bore", 0, 0, 0, -1, -1, 1590, 1550, false, false, false, {"300blk"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_450", ".450 长管上机匣", "rifle .450 upper receiver", "bore", 0, 15, 0, -1, -1, 2722, 1760, false, false, false, {"450"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_450_medium", ".450 CQB上机匣", "CQB .450 upper receiver", "bore", 0, 15, 0, -1, -1, 1900, 1295, false, false, false, {"450"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_450_short", ".450 短管上机匣", "SBR .450 upper receiver", "bore", 0, 15, 0, -1, -1, 1615, 850, false, false, false, {"450"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_50beowulf", "12.7x42mm 长管上机匣", "rifle 12.7x42mm upper receiver", "bore", 0, 15, 0, -1, -1, 2495, 1760, false, false, false, {"50beowulf"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_50beowulf_medium", "12.7x42mm 中长上机匣", "mid-length 12.7x42mm upper receiver", "bore", 0, 15, 0, -1, -1, 2341, 1550, false, false, false, {"50beowulf"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_50beowulf_short", "12.7x42mm 短管上机匣", "SBR 12.7x42mm upper receiver", "bore", 0, 15, 0, -1, -1, 1615, 850, false, false, false, {"50beowulf"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_ar15_762", "7.62x39mm 上机匣", "7.62x39mm upper receiver", "bore", 0, 0, 0, -1, -1, 2268, 1550, false, false, false, {"762"}, {"modular_ar15", "modular_m4_carbine", "modular_m16a4", "modular_m16_auto_rifle", "modular_m27_assault_rifle", "modular_ar_pistol"}, "core");
    add_gunmod("retool_axmc_300win", "AXMC .300 枪管套件", "AXMC .300 barrel-assembly", "bore", 0, 0, 0, -1, -1, 1000, 500, false, false, false, {"300"}, {"axmc"}, "core");
    add_gunmod("retool_axmc_308win", "AXMC .308 枪管套件", "AXMC .308 barrel-assembly", "bore", 0, 0, 0, -1, -1, 1000, 500, false, false, false, {"308"}, {"axmc"}, "core");
    add_gunmod("retool_axmc_338lapua", "AXMC .338 枪管套件", "AXMC .338 barrel-assembly", "bore", 0, 0, 0, -1, -1, 1000, 500, false, false, false, {"338lapua"}, {"axmc"}, "core");
    add_gunmod("retool_cz600_223rem", "CZ .223 枪管套件", "CZ .223 barrel-assembly", "bore", 0, 0, 0, -1, -1, 1000, 500, false, false, false, {"223"}, {"cz600"}, "core");
    add_gunmod("retool_cz600_762", "CZ 7.62x39mm 枪管套件", "CZ 7.62x39mm barrel-assembly", "bore", 0, 0, 0, -1, -1, 1000, 500, false, false, false, {"762"}, {"cz600"}, "core");
    add_gunmod("retool_deagle_357", "沙漠之鹰 .357 改装套件", "Desert Eagle .357 conversion kit", "bore", 0, 0, 0, -1, -1, 498, 550, false, false, false, {"357mag"}, {"modular_deagle"}, "core");
    add_gunmod("retool_deagle_44", "沙漠之鹰 .44 改装套件", "Desert Eagle .44 conversion kit", "bore", 4, 15, 0, -1, -1, 498, 550, false, false, false, {"44"}, {"modular_deagle"}, "core");
    add_gunmod("retool_deagle_50", "沙漠之鹰 .50 改装套件", "Desert Eagle .50 conversion kit", "bore", 4, 15, 0, -1, -1, 498, 550, false, false, false, {"50ae"}, {"modular_deagle"}, "core");
    add_gunmod("retool_mdrx_223rem", "MDRX .223 加长套件", "MDRX .223 extended-assembly", "bore", 4, 15, 0, -1, -1, 1000, 500, false, false, false, {"223"}, {"mdrx"}, "core");
    add_gunmod("retool_mdrx_223rem_medium", "MDRX .223 标准套件", "MDRX .223 standard-assembly", "bore", 4, 15, 0, -1, -1, 840, 420, false, false, false, {"223"}, {"mdrx"}, "core");
    add_gunmod("retool_mdrx_223rem_short", "MDRX .223 MICRON套件", "MDRX .223 MICRON assembly", "bore", 4, 15, 0, -1, -1, 680, 340, false, false, false, {"223"}, {"mdrx"}, "core");
    add_gunmod("retool_mdrx_300blk", "MDRX .300 BLK 标准套件", "MDRX .300BLK standard-assembly", "bore", 4, 15, 0, -1, -1, 840, 420, false, false, false, {"300blk"}, {"mdrx"}, "core");
    add_gunmod("retool_mdrx_308win", "MDRX .308 加长套件", "MDRX .308 extended-assembly", "bore", 4, 15, 0, -1, -1, 1000, 500, false, false, false, {"308"}, {"mdrx"}, "core");
    add_gunmod("retool_mdrx_308win_medium", "MDRX .308 标准套件", "MDRX .308 standard-assembly", "bore", 4, 15, 0, -1, -1, 840, 420, false, false, false, {"308"}, {"mdrx"}, "core");
    add_gunmod("retool_ump_40", "UMP .40 S&W 改装套件", "UMP .40 S&W conversion kit", "bore", 0, 0, 0, -1, -1, 1200, 700, false, false, false, {"40"}, {"modular_ump", "modular_ump_9", "modular_ump_40", "modular_ump_45"}, "core");
    add_gunmod("retool_ump_45", "UMP .45 ACP 改装套件", "UMP .45 ACP conversion kit", "bore", 0, 0, 0, -1, -1, 1400, 700, false, false, false, {"45"}, {"modular_ump", "modular_ump_9", "modular_ump_40", "modular_ump_45"}, "core");
    add_gunmod("retool_ump_9", "UMP 9x19mm 改装套件", "UMP 9x19mm conversion kit", "bore", 0, 0, 0, -1, -1, 1200, 700, false, false, false, {"9mm"}, {"modular_ump", "modular_ump_9", "modular_ump_40", "modular_ump_45"}, "core");
    add_gunmod("rifle_scope", "步枪瞄准镜", "rifle scope", "sights", 0, 0, -1, 0, 270, 669, 485, false, false, true, {}, {"rifle", "crossbow", "launcher", "shotgun"}, "core");
    add_gunmod("rifle_scope_high_end_mount", "高端步枪瞄准镜", "high end rifle scope", "sights", 0, 0, -1, 0, 270, 700, 485, false, false, true, {}, {"rifle", "crossbow", "launcher", "shotgun"}, "core");
    add_gunmod("riv_scope", "RS1219 瞄准镜", "RS1219 scope", "sights", 0, 0, 0, 0, 270, 0, 0, false, false, true, {}, {"rifle", "crossbow", "launcher", "shotgun"}, "core");
    add_gunmod("rm121aux_mod", "改装 RM121 管下霰弹枪", "modified RM121 aux shotgun", "underbarrel", 0, 0, 0, -1, -1, 1140, 750, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("robofac_gun_46", "HWP 自卫型（4.6x30mm）", "HWP personal 4.6x30mm configuration", "bore", 2, 0, 0, -1, -1, 500, 500, false, false, false, {"46"}, {"robofac_gun"}, "core");
    add_gunmod("robofac_gun_57", "HWP 自卫型(5.7x28mm)", "HWP personal 5.7x28mm configuration", "bore", 2, 0, 0, -1, -1, 500, 500, false, false, false, {"57"}, {"robofac_gun"}, "core");
    add_gunmod("robofac_gun_ar", "HWP 突击型(5.56x45mm)", "HWP assault configuration", "bore", 4, 15, 0, -1, -1, 750, 500, false, false, false, {"223"}, {"robofac_gun"}, "core");
    add_gunmod("robofac_gun_dmr", "HWP 精确射手型(7.62x51mm)", "HWP designated marksman configuration", "bore", 2, 12, 0, -1, -1, 1000, 500, false, false, false, {"308"}, {"robofac_gun"}, "core");
    add_gunmod("robofac_gun_exodii", "HWP 异域型(12.3ln)", "HWP EXOTIC configuration", "bore", 18, 0, 0, -1, -1, 2400, 1200, true, false, false, {"123ln"}, {"robofac_gun"}, "core");
    add_gunmod("robofac_gun_shotgun", "HWP 近战型（霰弹）", "HWP close quarters configuration", "bore", 0, 0, 0, -1, -1, 1000, 500, false, false, false, {"shot"}, {"robofac_gun"}, "core");
    add_gunmod("robofac_gun_shotgun_breach", "HWP 破门型（霰弹）", "HWP breacher configuration", "bore", 0, 0, 0, -1, -1, 1000, 500, false, false, false, {"shot"}, {"robofac_gun"}, "core");
    add_gunmod("robofac_gun_smg", "HWP 自卫型(9x19mm)", "HWP personal defense configuration", "bore", 2, 0, 0, -1, -1, 500, 500, false, false, false, {"9mm"}, {"robofac_gun"}, "core");
    add_gunmod("robofac_handguard", "内置前握把", "integrated front grip", "underbarrel", 6, 0, 0, -1, -1, 68, 119, false, false, false, {}, {"shotgun", "smg", "rifle", "crossbow", "launcher"}, "core");
    add_gunmod("robofac_laser_sight", "内置激光指示器", "integral laser designator", "rail", 0, 0, 15, 30, 3000, 120, 250, false, true, false, {}, {"smg", "rifle", "pistol", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("robofac_stock", "无托式可调枪托", "bullpup adjustable stock", "stock", 1, -1, 0, -1, -1, 350, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("scout_bipod", "集成聚合物两脚架", "integral polymer bipod", "underbarrel", 0, 0, 0, -1, -1, 0, 0, false, false, false, {}, {"steyr_scout"}, "core");
    add_gunmod("scout_bipod_deployed", "集成聚合物两脚架（展开）", "integral polymer bipod, deployed", "underbarrel", 12, 0, 0, -1, -1, 0, 0, true, false, false, {}, {"steyr_scout"}, "core");
    add_gunmod("scout_stock", "中空聚合物枪托", "hollow polymer stock", "stock", 0, 0, 0, -1, -1, 260, 500, false, false, false, {}, {"steyr_scout"}, "core");
    add_gunmod("shot_suppressor", "霰弹枪消音器", "shotgun suppressor", "muzzle", 2, 0, 0, -1, -1, 975, 1286, false, false, false, {}, {"shotgun"}, "core");
    add_gunmod("shoulder_strap", "可调式背带", "adjustable sling", "sling", 0, 0, 0, -1, -1, 100, 250, false, false, false, {}, {"rifle", "shotgun", "smg", "crossbow", "launcher", "pistol"}, "core");
    add_gunmod("shoulder_strap_front", "可调式背带（前）", "adjustable sling (front)", "sling", 0, 0, 0, -1, -1, 100, 250, false, false, false, {}, {"rifle", "shotgun", "smg", "crossbow", "launcher", "pistol"}, "core");
    add_gunmod("shoulder_strap_simple", "两点式背带", "two point sling", "sling", 0, 0, 0, -1, -1, 100, 250, false, false, false, {}, {"rifle", "shotgun", "smg", "crossbow", "launcher", "pistol"}, "core");
    add_gunmod("sights_mount", "瞄具底座", "sights mount", "sights mount", 0, 0, 0, -1, -1, 10, 8, false, false, false, {}, {"smg", "rifle", "shotgun", "crossbow"}, "core");
    add_gunmod("sights_mount_launcher", "发射器瞄具底座", "launcher sights mount", "sights mount", 0, 0, 0, -1, -1, 60, 80, false, false, false, {}, {"launcher"}, "core");
    add_gunmod("sights_mount_pistol", "手枪瞄具底座", "pistol sights mount", "sights mount", 0, 0, 0, -1, -1, 60, 80, false, false, false, {}, {"pistol"}, "core");
    add_gunmod("staff_sling_grenade_cradle", "手雷用投石杖托兜", "grenade cradle for staff sling", "bore", 0, 0, 0, -1, -1, 96, 250, false, false, false, {"sling-ready_grenade"}, {"staff_sling"}, "core");
    add_gunmod("stock_mauser", "毛瑟 C96 木制枪托", "Mauser C96 stock", "stock mount", 6, 0, 0, -1, -1, 590, 700, false, false, false, {}, {"mauser_c96"}, "core");
    add_gunmod("stock_mount", "现代化改装套件（枪托）", "replaceable stock kit", "stock mount", 0, 0, 0, -1, -1, 80, 50, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun", "slingshot", "crossbow", "launcher"}, "core");
    add_gunmod("stock_none", "锯短枪托", "sawn-off stock", "stock mount", -10, 0, 0, -1, -1, 0, 0, false, false, false, {}, {"smg", "rifle", "shotgun"}, "core");
    add_gunmod("sub2000_folding_mechanism", "集成折叠结构", "integral folding mechanism", "stock", 0, 0, 0, -1, -1, 10, 10, false, false, false, {}, {"ksub2000"}, "core");
    add_gunmod("sub2000_folding_mechanism_folded", "集成折叠结构（折叠）", "integral folding mechanism (folded))", "stock", 0, 0, 0, -1, -1, 10, 10, false, false, false, {}, {"ksub2000"}, "core");
    add_gunmod("suppressor", "消音器", "suppressor", "muzzle", 2, 0, 0, -1, -1, 550, 247, false, false, false, {}, {"pistol", "smg", "rifle"}, "core");
    add_gunmod("suppressor_compact", "手枪消音器", "pistol suppressor", "muzzle", 1, 0, 0, -1, -1, 303, 179, false, false, false, {}, {"pistol", "smg"}, "core");
    add_gunmod("sword_bayonet", "长刺刀", "sword bayonet", "bayonet lug", 0, 0, 0, -1, -1, 923, 293, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("tail_hook_stock", "臂箍枪托", "tail hook stock", "stock", 4, -20, 0, -1, -1, 150, 200, false, false, false, {}, {"smg", "pistol", "slingshot"}, "core");
    add_gunmod("tec9_auto_parts", "TEC-9自动阻铁", "TEC-9 full auto sear", "mechanism", 0, 0, 0, -1, -1, 55, 20, false, false, false, {}, {"tec9"}, "core");
    add_gunmod("tele_sight", "望远镜式瞄准镜", "telescopic sight", "sights", 0, 0, -1, 15, 270, 300, 250, false, false, true, {}, {"rifle", "shotgun", "crossbow", "launcher"}, "core");
    add_gunmod("tele_sight_pistol", "望远镜式手枪瞄准镜", "telescopic pistol sight", "sights", 0, 0, -1, 20, 270, 220, 150, false, false, true, {}, {"pistol", "smg"}, "core");
    add_gunmod("type99_scope", "九九式步枪瞄具", "Nagoya 99 gun scope", "sights", 0, 0, -1, 0, 270, 669, 485, false, false, true, {}, {"type99"}, "core");
    add_gunmod("under_folding_stock", "下折叠枪托", "under folding stock", "stock", 0, 0, 0, -1, -1, 100, 300, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher"}, "core");
    add_gunmod("under_folding_stock_folded", "下折叠枪托（折叠）", "under-folding stock (folded)", "stock", -10, 0, 0, -1, -1, 100, 300, false, false, false, {}, {"smg", "rifle", "shotgun", "launcher"}, "core");
    add_gunmod("underbarrel_mount", "管下底座", "bottom mount", "underbarrel mount", 0, 0, 0, -1, -1, 10, 8, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun", "crossbow", "launcher", "bow"}, "core");
    add_gunmod("upstest", "UPS test", "UPS test", "barrel", 0, 500, 0, -1, -1, 450, 500, false, false, false, {}, {"smg", "rifle", "shotgun", "pistol"}, "core");
    add_gunmod("waterproof_gunmod", "枪械防水套装", "firearm waterproofing", "mechanism", 0, 0, 0, -1, -1, 220, 250, false, false, false, {}, {"smg", "rifle", "pistol", "shotgun", "launcher"}, "core");
    add_gunmod("wire_stock", "铁丝折叠枪托", "collapsing wire stock", "stock", 0, 0, 0, -1, -1, 170, 250, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun"}, "core");
    add_gunmod("wire_stock_folded", "铁丝折叠枪托（折叠）", "collapsing wire stock (collapsed)", "stock", -10, 0, 0, -1, -1, 170, 250, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun"}, "core");
    add_gunmod("wooden_grip", "木制前置握把", "wood forward grip", "underbarrel", 6, 0, 0, -1, -1, 94, 119, false, false, false, {}, {"pistol", "smg", "rifle", "shotgun", "launcher", "crossbow"}, "core");
    add_gunmod("xedra_gun_ar", "膛线枪管", "rifled barrel", "bore", 4, 15, 0, -1, -1, 750, 500, false, false, false, {"223"}, {"xedra_gun"}, "core");
    add_gunmod("xedra_gun_shotgun", "无膛线枪管", "unrifled barrel", "bore", 0, 0, 0, -1, -1, 1000, 500, false, false, false, {"shot"}, {"xedra_gun"}, "core");
}
