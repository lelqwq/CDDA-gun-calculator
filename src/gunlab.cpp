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
#include "gunlab_math.h"  // 全部计算逻辑（gunlab_core）
#include "zh_cn.h"        // 全部中文文本

// 本文件只负责**命令行界面**：排版输出与交互。
// 所有公式都在 gunlab_math.cpp 里，图形界面版（src/gui/）共用同一套，
// 保证两边算出来的数字永远一致。

// ---- 第 1 部分数学工具 + 第 3 部分公式实现 已搬到 gunlab_math.cpp ----

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

// ---- 瞄准收益曲线 ------------------------------------------------------------
// 横轴 = 瞄准回合，纵轴 = 50%好击距离。
// 每回合推进 100 个行动点，记录该时刻的瞄准误差能打到多远。
// 曲线到顶后保持水平 —— 因为瞄准误差压到「瞄准精度上限」就降不下去了。
static void print_range_curve(const Gun& g, const Character& c, const Ammo* ammo)
{
    using namespace zh::t;

    const double fixed_disp = get_weapon_dispersion(g, c, ammo);

    AimContext ctx;
    ctx.len_factor = 1.0;
    ctx.limit      = most_accurate_aiming_method_limit(g, c);
    ctx.vol_factor = aim_factor_from_volume(g, effective_volume(g));

    const int TURN = 100;        // 一回合的行动点
    const int MAXT = 20;         // 最多画 20 回合

    std::vector<int> ys;         // ys[t] = 第 t 回合的 50%好击距离
    double recoil = MAX_RECOIL;
    int flat = 0;
    for( int t = 0; t <= MAXT; t++ ) {
        ys.push_back( range_with_even_chance_of_good_hit( fixed_disp + recoil ) );
        if( t > 0 && ys[t] == ys[t - 1] ) {
            if( ++flat >= 3 ) break;      // 连续 3 回合没变化就不再画了
        } else {
            flat = 0;
        }
        for( int i = 0; i < TURN && recoil > ctx.limit; i++ ) {
            const double amt = aim_per_move( g, c, recoil, ctx );
            if( amt <= 0 ) break;
            recoil = std::max( ctx.limit, recoil - amt );
        }
    }

    const int W    = (int)ys.size();
    const int maxY = *std::max_element( ys.begin(), ys.end() );
    const int H    = std::max( 1, std::min( maxY, 15 ) );   // 最多 15 行，避免刷屏

    std::cout << HDR_CURVE << NOTE_CURVE;

    // 行：从高到低。每列占 2 个字符宽，好让横轴刻度标得下
    for( int y = H; y >= 0; y-- ) {
        std::cout << pad( std::to_string( y ), 4 ) << "│";
        for( int x = 0; x < W; x++ ) {
            const int cur  = std::min( ys[x], H );
            const int prev = std::min( x > 0 ? ys[x - 1] : cur, H );
            const int lo = std::min( cur, prev );
            const int hi = std::max( cur, prev );
            if( y == cur ) {
                // 与右邻点同高就连一条横线，让平坦段看起来是折线而不是散点
                const bool flat_next = ( x + 1 < W ) && ( std::min( ys[x + 1], H ) == y );
                std::cout << ( flat_next ? "◆─" : "◆ " );
            } else if( y > lo && y < hi ) {
                std::cout << "│ ";
            } else {
                std::cout << "  ";
            }
        }
        std::cout << "\n";
    }

    // 横轴
    std::cout << "    └";
    for( int x = 0; x < W; x++ ) std::cout << "──";
    std::cout << "→ " << C_TURN << "\n     ";
    for( int x = 0; x < W; x++ ) {
        std::cout << pad( std::to_string( x ), 2 );
    }
    std::cout << "\n\n";

    // 数据表（图看不清时看这个）
    std::cout << "      " << pad( C_TURN, 8 ) << pad( LV_50RANGE, 16 ) << "\n";
    for( int x = 0; x < W; x++ ) {
        std::cout << "      " << pad( std::to_string( x ), 8 )
                  << pad( std::to_string( ys[x] ) + C_RANGE_AXIS, 16 ) << "\n";
    }
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

// ---- 搜索 / 弹药 / 枪的类型 已搬到 gunlab_math.cpp ----

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
    print_range_curve(gun, ch, ammo);
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
