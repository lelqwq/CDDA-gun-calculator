// =============================================================================
//  main_gui.cpp  —  图形界面版（SDL3 + ImGui）
// -----------------------------------------------------------------------------
//  与命令行版（src/gunlab.cpp）共用 gunlab_core，所以两边算出来的数字一致。
//  界面只负责「把数字摆出来」，不做任何计算 —— 改公式请去 gunlab_math.cpp。
//
//  渲染用 SDL3 自带的 SDL_Renderer（不是 OpenGL），省掉 GL 函数加载，
//  依赖更少，对这种数据工具完全够用。
//
//  ★ 中文字体必须手动加载，否则全是方块（ImGui 内置字体不含 CJK）。
//
//  ★ 概率表每档采样 20 万次，四个档位合计 80 万次，实测约 99 ms（Release）。
//    单独跑一次不算什么，但绝不能每帧都跑，所以做了两层处理：
//      1. 结果缓存，只在「枪 / 弹药 / 人物参数」变化时重算；
//      2. 只在「命中档位概率」那一栏展开时才算 —— 没展开就一分钱不花。
//    人物参数也因此用 InputInt 的 +/- 按钮而不是拖动条：拖动条每帧都在变，
//    会每帧触发重算，界面直接卡死。
//
//  命令行：gunlab_gui.exe [关键词]
//    给了关键词就直接选中第一条匹配的枪（匹配规则与搜索框相同），
//    方便从快捷方式直接跳到常用枪，也方便自动化验证截图。
// =============================================================================

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#ifndef SDL_MAIN_HANDLED
#  define SDL_MAIN_HANDLED      // 自己接管 main，不让 SDL 重定向到 SDL_main
#endif
#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "gun_data.h"
#include "gunlab_math.h"
#include "zh_cn.h"

// =============================================================================
//  中文字体
// =============================================================================
//  ImGui 内置字体只有 ASCII+拉丁，不加载 CJK 字体的话中文全是方块。
//  按优先级找一个能用的：系统雅黑 → 黑体 → 宋体 → 游戏自带 unifont。
static void load_cjk_font( ImGuiIO &io, float size )
{
    const char *candidates[] = {
        "C:/Windows/Fonts/msyh.ttc",     // 微软雅黑（最好看）
        "C:/Windows/Fonts/msyh.ttf",
        "C:/Windows/Fonts/simhei.ttf",   // 黑体
        "C:/Windows/Fonts/simsun.ttc",   // 宋体
        "C:/Windows/Fonts/Deng.ttf",     // 等线
        "data/font/unifont.ttf",         // 游戏自带（点阵，较丑）
    };

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;

    for( const char *path : candidates ) {
        std::error_code ec;
        if( !std::filesystem::exists( path, ec ) ) {
            continue;
        }
        if( io.Fonts->AddFontFromFileTTF( path, size, &cfg,
                                          io.Fonts->GetGlyphRangesChineseFull() ) != nullptr ) {
            std::printf( "已加载中文字体：%s\n", path );
            return;
        }
    }
    std::printf( "警告：没找到中文字体，界面上的中文会显示为方块\n" );
    io.Fonts->AddFontDefault();
}

// =============================================================================
//  界面
// =============================================================================

namespace {

// 标签列的固定宽度：所有 "标签 —— 数值" 的行都对齐到这里
constexpr float LABEL_W = 220.0f;
// 同级的空格间距
constexpr float GAP     = 18.0f;

// ---- 全局状态 ---------------------------------------------------------------

char             g_search[128] = "";
std::vector<int> g_hits;                  // 搜索命中的枪械下标
int              g_selected = -1;         // 当前选中的枪械下标

// 列表排序。点列标题切换，再点一下反向，点第三下取消（ImGui 的行为）。
// -1 = 默认顺序，也就是 search_guns 给的顺序（按 id，即数据库顺序）。
int  g_sort_col  = -1;
bool g_sort_desc = false;

// 技能筛选。-1 = 全部；否则是 g_skill_keys 的下标。
// 和搜索框是「与」的关系：先按关键词搜，再按技能过滤。
//
// 只在图形版做 —— 命令行版保持原样，它在终端里靠关键词已经够用了。
std::vector<std::string> g_skill_keys;     // 数据里实际出现的技能（逻辑键）
std::vector<int>         g_skill_counts;   // 各自的枪械总数
int                      g_skill_filter = -1;

// 选中枪械的工作副本。装/卸配件改的是它，g_guns 那份只读数据库保持原样 ——
// 否则换个枪再换回来，之前装的配件会「粘」在数据库上。
Gun g_work;
int g_work_rev = 0;                       // 每改一次配件 +1，用来让详情缓存失效

std::vector<int> g_ammo_choices;          // 当前枪可用的弹药（g_ammo 的下标）
int              g_ammo_pick = -1;        // 在 g_ammo_choices 里的位置

Character g_ch;                           // 人物（详情里所有数字都随它变）
int g_p_dex = 8, g_p_per = 8, g_p_str = 8;   // 界面上可编辑的那几个
int g_p_skill = 0, g_p_marks = 0;

// ---- 详情缓存 ---------------------------------------------------------------
//  两段缓存分开：基础数据很便宜，每次选中就重算；概率表很贵，只在展开时才算。

struct DetailCache {
    bool   valid    = false;
    int    gun_idx  = -1;
    int    ammo_idx = -1;
    int    work_rev = -1;                     // 配件改动也要让缓存失效
    int    dex = 0, per = 0, str = 0, skill = 0, marks = 0;

    AimResult aim;
    double    fixed_disp = 0.0;
    int       aim_range[3] = { 0, 0, 0 };     // 三个档位各自的 50%好击距离
    double    accuracy_limit = 0.0;
    double    added_recoil   = 0.0;

    // 逐回合的两条曲线，下标都是「第 t 回合」：
    //   curve[t]        = 这个瞄准误差能打到多远（50%好击距离，格）
    //   recoil_curve[t] = 回合开始时的瞄准误差
    // 两条曲线出自同一趟循环，所以一一对应。
    std::vector<double> curve;
    std::vector<double> recoil_curve;
};

constexpr int    PROB_N      = 200000;
constexpr double PROB_TARGET = 1.0;
constexpr unsigned PROB_SEED = 20260910u;   // 固定种子，和命令行版一致
constexpr int    PROB_NDIST  = 7;
constexpr int    PROB_NTIER  = 6;
constexpr int    PROB_NLEVEL = 4;
const double     PROB_DISTS[PROB_NDIST] = { 1, 2, 5, 10, 20, 30, 40 };

struct ProbCache {
    bool valid    = false;
    int  gun_idx  = -1;
    int  ammo_idx = -1;
    int  work_rev = -1;
    int  dex = 0, per = 0, str = 0, skill = 0, marks = 0;
    double th[PROB_NLEVEL] = { 0, 0, 0, 0 };                 // 各档位的瞄准误差
    double pct[PROB_NLEVEL][PROB_NDIST][PROB_NTIER] = {};    // 百分比
};

DetailCache g_detail;
ProbCache   g_probs;

// ---- 小工具 -----------------------------------------------------------------

// 界面上的文案大多带 %d / %.1f 之类的占位符，先格式化再交给 ImGui。
// 不用 std::format 是因为 GCC 16 的 libstdc++ 才有，MSVC 侧版本不一致。
std::string fmt_str( const char *fmt, ... )
{
    char buf[512];
    va_list ap;
    va_start( ap, fmt );
    std::vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );
    return std::string( buf );
}

// 把光标移到「上一段文字右侧」：至少 min_x，文字比 min_x 还宽时再往后让。
//
// ★ 不要直接用 ImGui::SameLine(固定值) 对齐 —— 上一段文字一旦超过那个位置，
//   下一段就会画到它上面去。中文标签特别容易超：一个汉字约 18px，
//   「一回合（100 行动点）后瞄准误差降到」就有 300px 了，而 LABEL_W 才 220。
void same_line_after( const char *rendered, float min_x = LABEL_W )
{
    ImGui::SameLine( std::max( min_x, ImGui::CalcTextSize( rendered ).x + GAP ) );
}

// 「标签 ←(对齐到 LABEL_W)→ 数值」
void kv( const char *label, const char *fmt, ... )
{
    ImGui::TextUnformatted( label );
    same_line_after( label );

    char buf[512];
    va_list ap;
    va_start( ap, fmt );
    std::vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );
    ImGui::TextUnformatted( buf );
}

// 给坐标轴挑一个「好看」的刻度步长：结果一定是 1/2/5 × 10ⁿ，
// 这样刻度标签是 5 / 10 / 20 而不是 7 / 14 / 21
double nice_step( double range, int target_ticks )
{
    if( range <= 0.0 || target_ticks <= 0 ) {
        return 1.0;
    }
    const double raw  = range / target_ticks;
    const double mag  = std::pow( 10.0, std::floor( std::log10( raw ) ) );
    const double norm = raw / mag;
    const double pick = ( norm <= 1.0 ) ? 1.0
                      : ( norm <= 2.0 ) ? 2.0
                      : ( norm <= 5.0 ) ? 5.0
                      : 10.0;
    return pick * mag;
}

// 折线图上的参考横线（比如三个瞄准档位的阈值）
struct ChartMark {
    double      y;
    const char *label;
};

// 定义在下面「瞄准档位实例」附近。这里前置声明是因为瞄准时间线的图要用它，
// 而那个函数定义在更前面。
void draw_line_chart( const char *id,
                      const std::vector<double> &data,
                      const char *y_label,
                      const char *y_fmt,
                      const char *tip_fmt,
                      const std::vector<ChartMark> &marks = {} );

// 灰色小字说明，自动换行
void note( const char *fmt, ... )
{
    char buf[1024];
    va_list ap;
    va_start( ap, fmt );
    std::vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );

    ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled] );
    ImGui::Indent();
    ImGui::PushTextWrapPos( 0.0f );
    ImGui::TextUnformatted( buf );
    ImGui::PopTextWrapPos();
    ImGui::Unindent();
    ImGui::PopStyleColor();
}

// ---- 选择与缓存 -------------------------------------------------------------

const Ammo *current_ammo()
{
    if( g_ammo_pick < 0 || g_ammo_pick >= (int)g_ammo_choices.size() ) {
        return nullptr;
    }
    return &g_ammo[g_ammo_choices[g_ammo_pick]];
}

// 把界面上的整数参数同步进 Character
void sync_character()
{
    g_ch.dex  = g_p_dex;
    g_ch.per  = g_p_per;
    g_ch.str  = g_p_str;
    g_ch.skill_level        = g_p_skill;
    g_ch.marksmanship_level = g_p_marks;
}

// 缓存键：枪 / 弹药 / 配件改动次数 / 四个人物参数，任何一项变了都得重算
template<typename T>
bool key_matches( const T &c )
{
    return c.valid && c.gun_idx == g_selected && c.ammo_idx == g_ammo_pick
        && c.work_rev == g_work_rev
        && c.dex == g_p_dex && c.per == g_p_per && c.str == g_p_str
        && c.skill == g_p_skill && c.marks == g_p_marks;
}

template<typename T>
void store_key( T &c )
{
    c.gun_idx  = g_selected;
    c.ammo_idx = g_ammo_pick;
    c.work_rev = g_work_rev;
    c.dex = g_p_dex;  c.per = g_p_per;  c.str = g_p_str;
    c.skill = g_p_skill;  c.marks = g_p_marks;
}

// 重算这把枪可选哪些弹药。keep_current 为 true 时尽量留住当前选中的那款 ——
// 装个配件不该顺手把你挑好的弹药换掉；实在没了（比如换了机匣改口径）才回落。
void refresh_ammo_choices( bool keep_current )
{
    std::string prev_id;
    if( keep_current ) {
        if( const Ammo *prev = current_ammo() ) {
            prev_id = prev->id;
        }
    }

    g_ammo_choices = ammo_for_gun( g_work );
    g_ammo_pick = -1;

    if( !prev_id.empty() ) {
        for( int i = 0; i < (int)g_ammo_choices.size(); i++ ) {
            if( g_ammo[g_ammo_choices[i]].id == prev_id ) { g_ammo_pick = i; break; }
        }
    }

    // 默认选「标准弹」（普通 FMJ），选不到就取第一种
    if( g_ammo_pick < 0 ) {
        if( const Ammo *def = pick_default_ammo( g_work ) ) {
            // pick_default_ammo 返回的是 g_ammo 里元素的地址，换成下标
            const int di = (int)( def - g_ammo.data() );
            if( di >= 0 && di < (int)g_ammo.size() ) {
                for( int i = 0; i < (int)g_ammo_choices.size(); i++ ) {
                    if( g_ammo_choices[i] == di ) { g_ammo_pick = i; break; }
                }
            }
        }
    }
    if( g_ammo_pick < 0 && !g_ammo_choices.empty() ) {
        g_ammo_pick = 0;
    }
}

// 配件有变动：两个缓存作废，并重算可选弹药（机匣会改口径）
void mods_changed()
{
    g_work_rev++;
    g_detail.valid = false;
    g_probs.valid  = false;
    refresh_ammo_choices( true );
}

// 装上配件。一个槽位只能装一件，同槽位的旧配件自动替换 —— 与命令行版一致
void install_mod( int mod_idx )
{
    if( mod_idx < 0 || mod_idx >= (int)g_mods.size() ) {
        return;
    }
    // 弹窗里已经按 mod_fits_gun 过滤过了，这里再挡一道 —— 装配是改状态的操作，
    // 不该依赖调用方有没有先过滤
    if( !mod_fits_gun( g_work, g_mods[mod_idx] ) ) {
        return;
    }
    const GunMod nm = g_mods[mod_idx];
    g_work.mods.erase( std::remove_if( g_work.mods.begin(), g_work.mods.end(),
                                       [&]( const GunMod &x ) {
                                           return x.location == nm.location;
                                       } ),
                       g_work.mods.end() );
    g_work.mods.push_back( nm );
    mods_changed();
}

void remove_mod_at( int which )
{
    if( which < 0 || which >= (int)g_work.mods.size() ) {
        return;
    }
    g_work.mods.erase( g_work.mods.begin() + which );
    mods_changed();
}

void select_gun( int idx )
{
    g_selected = idx;
    g_detail.valid = false;
    g_probs.valid  = false;
    g_work_rev = 0;
    g_ammo_choices.clear();
    g_ammo_pick = -1;
    if( idx < 0 ) {
        return;
    }

    g_work = g_guns[idx];         // 工作副本，改配件只动它
    sync_character();
    // 和命令行版一致：武器技能固定取这把枪对应的技能
    g_ch.gun_skill = g_work.skill;

    refresh_ammo_choices( false );
}

// 按当前排序列重排 g_hits。
//
// 用 stable_sort：技能这种有大量同值的列，同组内会自动保持原有顺序
// （默认是按 id），不会每次点都抖一下。
//
// ★ 名称和技能比的都是**界面上显示的那串文字**（中文），所以是按 UTF-8
//   码点比的 —— 汉字落在 CJK 统一表意文字区，那个区段本身就是按
//   「部首 + 笔画」排的，所以结果是字典的部首序，不是拼音序。
//   好处是所见即所排；代价是别指望按拼音找枪，那个用搜索框更合适。
void apply_sort()
{
    if( g_sort_col < 0 ) {
        return;                      // 默认顺序，不动
    }

    const bool desc = g_sort_desc;
    std::stable_sort( g_hits.begin(), g_hits.end(), [desc]( int a, int b ) {
        const Gun &ga = g_guns[a];
        const Gun &gb = g_guns[b];

        int cmp = 0;
        switch( g_sort_col ) {
            case 0:                                   // 名称
                cmp = ga.name.compare( gb.name );
                break;
            case 1:                                   // 技能
                cmp = zh::skill( ga.skill ).compare( zh::skill( gb.skill ) );
                break;
            case 2: {                                 // 重量
                const double wa = effective_weight( ga );
                const double wb = effective_weight( gb );
                cmp = ( wa < wb ) ? -1 : ( wa > wb ) ? 1 : 0;
                break;
            }
            default:
                break;
        }
        return desc ? ( cmp > 0 ) : ( cmp < 0 );
    } );
}

// 建技能索引：扫一遍数据库，统计每种技能有多少把枪。
// 顺序按数量降序 —— 下拉框里最常用的（步枪 161 把）排最前，好找。
void build_skill_index()
{
    for( const Gun &g : g_guns ) {
        size_t i = 0;
        for( ; i < g_skill_keys.size(); i++ ) {
            if( g_skill_keys[i] == g.skill ) {
                g_skill_counts[i]++;
                break;
            }
        }
        if( i == g_skill_keys.size() ) {
            g_skill_keys.push_back( g.skill );
            g_skill_counts.push_back( 1 );
        }
    }

    // 数量降序；数量相同按显示名，保证顺序稳定，不会每次启动都不一样
    std::vector<size_t> order( g_skill_keys.size() );
    for( size_t i = 0; i < order.size(); i++ ) {
        order[i] = i;
    }
    std::stable_sort( order.begin(), order.end(), []( size_t a, size_t b ) {
        if( g_skill_counts[a] != g_skill_counts[b] ) {
            return g_skill_counts[a] > g_skill_counts[b];
        }
        return zh::skill( g_skill_keys[a] ) < zh::skill( g_skill_keys[b] );
    } );

    std::vector<std::string> keys;
    std::vector<int>         counts;
    keys.reserve( order.size() );
    counts.reserve( order.size() );
    for( size_t i : order ) {
        keys.push_back( g_skill_keys[i] );
        counts.push_back( g_skill_counts[i] );
    }
    g_skill_keys.swap( keys );
    g_skill_counts.swap( counts );
}

void refresh_hits()
{
    g_hits = search_guns( g_search );

    // 技能筛选在搜索之后做 —— 两者是「与」
    if( g_skill_filter >= 0 && g_skill_filter < (int)g_skill_keys.size() ) {
        const std::string &want = g_skill_keys[g_skill_filter];
        g_hits.erase( std::remove_if( g_hits.begin(), g_hits.end(),
                                      [&want]( int i ) {
                                          return g_guns[i].skill != want;
                                      } ),
                      g_hits.end() );
    }

    apply_sort();                    // 换了关键词/筛选要按当前排序列重新排
}

// ---- 缓存计算 ---------------------------------------------------------------

void compute_detail( const Gun &g, const Ammo *ammo )
{
    g_detail.valid = false;

    AimContext ctx;
    ctx.len_factor = 1.0;                 // 与命令行版一致：空旷地形
    g_detail.aim        = simulate_aim( g, g_ch, ctx );
    g_detail.fixed_disp = get_weapon_dispersion( g, g_ch, ammo );
    g_detail.accuracy_limit = most_accurate_aiming_method_limit( g, g_ch );
    g_detail.added_recoil   = added_recoil_per_shot(
        gun_recoil( g, g_ch.str, ammo ? ammo->recoil : 0.0 ),
        recoil_absorb( g_ch.skill_level ) );

    const double th[3] = { g_detail.aim.regular_th, g_detail.aim.careful_th,
                           g_detail.aim.precise_th };
    for( int i = 0; i < 3; i++ ) {
        g_detail.aim_range[i] =
            range_with_even_chance_of_good_hit( g_detail.fixed_disp + th[i] );
    }

    // ---- 逐回合推进瞄准 -----------------------------------------------------
    // 一趟循环同时产出两条曲线：这一回合能打多远，以及回合开始时的瞄准误差。
    // 与命令行版 print_range_curve 逐行对应（含 ctx 的取法）—— 两边必须
    // 给出一条一样的曲线。
    {
        AimContext cctx;
        cctx.len_factor = 1.0;
        cctx.limit      = g_detail.accuracy_limit;
        cctx.vol_factor = aim_factor_from_volume( g, effective_volume( g ) );

        const int TURN = 100;     // 一回合的行动点
        const int MAXT = 20;      // 最多算 20 回合
        g_detail.curve.clear();
        g_detail.recoil_curve.clear();

        double recoil = MAX_RECOIL;
        int    flat   = 0;
        for( int t = 0; t <= MAXT; t++ ) {
            g_detail.curve.push_back(
                range_with_even_chance_of_good_hit( g_detail.fixed_disp + recoil ) );
            g_detail.recoil_curve.push_back( recoil );

            const size_t n = g_detail.curve.size();
            if( t > 0 && g_detail.curve[n - 1] == g_detail.curve[n - 2] ) {
                if( ++flat >= 3 ) {
                    break;         // 连续 3 回合没变化，再瞄也没意义了
                }
            } else {
                flat = 0;
            }

            for( int i = 0; i < TURN && recoil > cctx.limit; i++ ) {
                const double amt = aim_per_move( g, g_ch, recoil, cctx );
                if( amt <= 0 ) {
                    break;
                }
                recoil = std::max( cctx.limit, recoil - amt );
            }
        }
    }

    store_key( g_detail );
    g_detail.valid = true;
}

void compute_probs( const Gun &g, const Ammo *ammo, const AimResult &ar )
{
    g_probs.valid = false;

    g_probs.th[0] = MAX_RECOIL;
    g_probs.th[1] = ar.regular_th;
    g_probs.th[2] = ar.careful_th;
    g_probs.th[3] = ar.precise_th;

    std::mt19937 rng( PROB_SEED );
    std::vector<double> samples( PROB_N );

    for( int L = 0; L < PROB_NLEVEL; L++ ) {
        for( int i = 0; i < PROB_N; i++ ) {
            samples[i] = roll_dispersion( g, g_ch, ammo, g_probs.th[L], rng );
        }
        for( int d = 0; d < PROB_NDIST; d++ ) {
            long cnt[PROB_NTIER] = { 0, 0, 0, 0, 0, 0 };
            for( int i = 0; i < PROB_N; i++ ) {
                cnt[tier_index( missed_by( samples[i], PROB_DISTS[d], PROB_TARGET ) )]++;
            }
            for( int t = 0; t < PROB_NTIER; t++ ) {
                g_probs.pct[L][d][t] = 100.0 * (double)cnt[t] / PROB_N;
            }
        }
    }

    store_key( g_probs );
    g_probs.valid = true;
}

// ---- 顶部：搜索 + 人物参数 ---------------------------------------------------

void draw_toolbar()
{
    ImGui::SetNextItemWidth( 320 );
    if( ImGui::InputTextWithHint( "##search", zh::g::SEARCH_HINT,
                                  g_search, sizeof( g_search ) ) ) {
        refresh_hits();
        select_gun( -1 );
    }
    ImGui::SameLine();
    if( ImGui::Button( zh::g::CLEAR ) ) {
        g_search[0] = '\0';
        refresh_hits();
        select_gun( -1 );
    }
    ImGui::SameLine();
    ImGui::TextDisabled( zh::g::COUNT_FMT, (int)g_hits.size(), (int)g_guns.size() );

    // 技能筛选下拉框
    ImGui::SameLine( 0, GAP * 2 );
    ImGui::TextUnformatted( zh::g::SKILL_FILTER );
    ImGui::SameLine();

    const std::string cur = ( g_skill_filter < 0 )
                            ? fmt_str( zh::g::SKILL_ALL_FMT, (int)g_guns.size() )
                            : fmt_str( zh::g::SKILL_ITEM_FMT,
                                       zh::skill( g_skill_keys[g_skill_filter] ).c_str(),
                                       g_skill_counts[g_skill_filter] );
    ImGui::SetNextItemWidth( 170 );
    if( ImGui::BeginCombo( "##skillfilter", cur.c_str() ) ) {
        if( ImGui::Selectable( fmt_str( zh::g::SKILL_ALL_FMT,
                                        (int)g_guns.size() ).c_str(),
                               g_skill_filter < 0 ) ) {
            g_skill_filter = -1;
            refresh_hits();
        }
        for( int i = 0; i < (int)g_skill_keys.size(); i++ ) {
            ImGui::PushID( i );
            if( ImGui::Selectable(
                    fmt_str( zh::g::SKILL_ITEM_FMT,
                             zh::skill( g_skill_keys[i] ).c_str(),
                             g_skill_counts[i] ).c_str(),
                    g_skill_filter == i ) ) {
                g_skill_filter = i;
                refresh_hits();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::TextDisabled( "%s", zh::g::CHAR_HINT );
    ImGui::SameLine();
    ImGui::TextDisabled( "|" );

    // 用 InputInt（带 +/- 按钮）而不是 SliderInt —— 见文件头的说明
    ImGui::SameLine();
    ImGui::TextUnformatted( zh::g::P_DEX );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 90 );
    ImGui::InputInt( "##dex", &g_p_dex, 1, 1 );

    ImGui::SameLine( 0, GAP );
    ImGui::TextUnformatted( zh::g::P_PER );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 90 );
    ImGui::InputInt( "##per", &g_p_per, 1, 1 );

    ImGui::SameLine( 0, GAP );
    ImGui::TextUnformatted( zh::g::P_STR );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 90 );
    ImGui::InputInt( "##str", &g_p_str, 1, 1 );

    // 武器技能的标签随枪种变（选中步枪时显示「步枪等级」）
    //
    // ★ 这里必须存 std::string 而不是 c_str()。zh::skill() 返回的是临时
    //   std::string，用 `const char *p = cond ? zh::skill(x).c_str() : "…"`
    //   的话，临时对象在整条语句结束时就析构了，p 立刻变成悬垂指针。
    const std::string gun_skill_name = ( g_selected >= 0 )
                                       ? zh::skill( g_work.skill )
                                       : std::string( zh::g::P_SKILL );
    ImGui::SameLine( 0, GAP );
    ImGui::Text( zh::g::P_SKILL_FMT, gun_skill_name.c_str() );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 90 );
    ImGui::InputInt( "##skill", &g_p_skill, 1, 1 );

    ImGui::SameLine( 0, GAP );
    ImGui::TextUnformatted( zh::g::P_MARKS );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 90 );
    ImGui::InputInt( "##marks", &g_p_marks, 1, 1 );

    sync_character();
    if( g_selected >= 0 ) {
        g_ch.gun_skill = g_work.skill;
    }
}

// ---- 左侧：枪械列表 ---------------------------------------------------------

void draw_gun_list()
{
    ImGui::BeginChild( "list", ImVec2( 380, 0 ), ImGuiChildFlags_Borders );
    if( ImGui::BeginTable( "guns", 3,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                           ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
                           // 三态：升序 → 降序 → 取消。没有它就回不到默认顺序
                           // （按 id，即数据库顺序）了，只能重启程序
                           ImGuiTableFlags_SortTristate ) ) {
        ImGui::TableSetupScrollFreeze( 0, 1 );
        ImGui::TableSetupColumn( zh::g::COL_NAME, ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( zh::g::COL_SKILL, ImGuiTableColumnFlags_WidthFixed, 64 );
        ImGui::TableSetupColumn( zh::g::COL_WEIGHT, ImGuiTableColumnFlags_WidthFixed, 80 );
        ImGui::TableHeadersRow();

        // 排序状态只能在 TableHeadersRow() 之后读 —— 表头是那一步画的，
        // 点击也是那一步处理的。重排要放在下面遍历 g_hits 之前。
        if( ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs() ) {
            specs->SpecsDirty = false;
            if( specs->SpecsCount > 0 ) {
                const int  col  = (int)specs->Specs[0].ColumnIndex;
                const bool desc = ( specs->Specs[0].SortDirection
                                    == ImGuiSortDirection_Descending );
                if( col != g_sort_col || desc != g_sort_desc ) {
                    g_sort_col  = col;
                    g_sort_desc = desc;
                    apply_sort();
                }
            } else if( g_sort_col >= 0 ) {
                // 点第三下取消排序，回到默认顺序
                g_sort_col  = -1;
                g_sort_desc = false;
                g_hits = search_guns( g_search );
            }
        }

        for( int idx : g_hits ) {
            const Gun &g = g_guns[idx];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID( idx );
            if( ImGui::Selectable( g.name.c_str(), g_selected == idx,
                                   ImGuiSelectableFlags_SpanAllColumns ) ) {
                select_gun( idx );
            }
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted( zh::skill( g.skill ).c_str() );
            ImGui::TableNextColumn();
            ImGui::Text( "%.0f g", effective_weight( g ) );
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

// ---- 右侧：详情 -------------------------------------------------------------

// 抬头 + 弹药选择
void draw_detail_header( const Gun &g )
{
    ImGui::TextUnformatted( g.name.c_str() );
    ImGui::SameLine();
    ImGui::TextDisabled( "(%s)", g.id.c_str() );
    ImGui::Separator();

    ImGui::Text( "%s：%s", zh::g::F_SKILL, zh::skill( g.skill ).c_str() );
    ImGui::SameLine( 0, GAP * 2 );
    ImGui::Text( "%s：%.0f g", zh::g::F_WEIGHT, effective_weight( g ) );
    ImGui::SameLine( 0, GAP * 2 );
    ImGui::Text( "%s：%.0f ml", zh::g::F_VOLUME, effective_volume( g ) );

    // 弹药下拉框：换弹药会重算所有数值
    ImGui::TextUnformatted( zh::g::F_AMMO );
    ImGui::SameLine();
    if( g_ammo_choices.empty() ) {
        ImGui::TextDisabled( "%s", zh::g::NO_AMMO );
        return;
    }

    const Ammo *cur = current_ammo();
    ImGui::SetNextItemWidth( 420 );
    if( ImGui::BeginCombo( "##ammo", cur ? cur->name.c_str() : "" ) ) {
        for( int i = 0; i < (int)g_ammo_choices.size(); i++ ) {
            const Ammo &a = g_ammo[g_ammo_choices[i]];
            ImGui::PushID( i );
            if( ImGui::Selectable( a.name.c_str(), g_ammo_pick == i ) ) {
                g_ammo_pick = i;
                g_detail.valid = false;
                g_probs.valid  = false;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if( cur ) {
        ImGui::SameLine();
        ImGui::TextDisabled( "(后坐 %.0f / 散布 %.0f)", cur->recoil, cur->dispersion );
    }
}

// 配件：已装的可以移除，未装的可从兼容列表里装
void draw_mods( const Gun &g )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_MODS, ImGuiTreeNodeFlags_DefaultOpen ) ) {
        return;
    }

    // ---- 已安装 -------------------------------------------------------------
    // 先把要执行的操作记下来，等表格画完再动手 —— 在遍历 g.mods 的过程中
    // 改 g.mods 会让迭代器失效
    int remove_which = -1;

    if( g.mods.empty() ) {
        ImGui::TextDisabled( "%s", zh::g::NO_MODS );
    } else if( ImGui::BeginTable( "mods", 7,
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV ) ) {
        ImGui::TableSetupColumn( zh::g::COL_MOD_NAME, ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( zh::g::COL_MOD_SLOT, ImGuiTableColumnFlags_WidthFixed, 100 );
        ImGui::TableSetupColumn( zh::g::COL_HANDLING, ImGuiTableColumnFlags_WidthFixed, 64 );
        ImGui::TableSetupColumn( zh::g::COL_AIM, ImGuiTableColumnFlags_WidthFixed, 64 );
        ImGui::TableSetupColumn( zh::g::COL_SIGHT, ImGuiTableColumnFlags_WidthFixed, 84 );
        ImGui::TableSetupColumn( zh::g::COL_FOV, ImGuiTableColumnFlags_WidthFixed, 64 );
        ImGui::TableSetupColumn( "##del", ImGuiTableColumnFlags_WidthFixed, 60 );
        ImGui::TableHeadersRow();

        for( int i = 0; i < (int)g.mods.size(); i++ ) {
            const GunMod &m = g.mods[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted( m.name.c_str() );
            ImGui::TableNextColumn(); ImGui::TextUnformatted( zh::slot( m.location ).c_str() );
            ImGui::TableNextColumn(); ImGui::Text( "%+.0f", m.handling_modifier );
            ImGui::TableNextColumn(); ImGui::Text( "%+.0f", m.aim_speed_modifier );
            ImGui::TableNextColumn();
            if( m.sight_dispersion >= 0 ) ImGui::Text( "%.0f", m.sight_dispersion );
            else                          ImGui::TextDisabled( "—" );
            ImGui::TableNextColumn();
            if( m.field_of_view >= 0 ) ImGui::Text( "%.0f", m.field_of_view );
            else                       ImGui::TextDisabled( "—" );
            ImGui::TableNextColumn();
            ImGui::PushID( i );
            if( ImGui::SmallButton( zh::g::REMOVE_MOD ) ) {
                remove_which = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // ---- 安装 ---------------------------------------------------------------
    ImGui::Spacing();
    if( ImGui::Button( zh::g::ADD_MOD ) ) {
        ImGui::OpenPopup( "mod_picker" );
    }

    if( ImGui::BeginPopup( "mod_picker" ) ) {
        ImGui::TextUnformatted( zh::g::MOD_PICKER );
        ImGui::Separator();
        note( "%s", zh::g::MOD_REPLACE );
        ImGui::Spacing();

        // 按槽位分组列兼容配件。同槽位的挨在一起，一眼看出哪几个在抢同一个位置
        const std::vector<std::string> slots = available_slots( g );
        bool any = false;
        ImGui::BeginChild( "mod_scroll", ImVec2( 520, 420 ) );
        for( const std::string &slot : slots ) {
            bool head = false;
            for( int i = 0; i < (int)g_mods.size(); i++ ) {
                const GunMod &m = g_mods[i];
                if( m.location != slot || !mod_fits_gun( g, m ) ) {
                    continue;
                }
                if( !head ) {
                    ImGui::SeparatorText( zh::slot( slot ).c_str() );
                    head = true;
                    any = true;
                }
                ImGui::PushID( i );
                if( ImGui::Selectable( m.name.c_str() ) ) {
                    install_mod( i );
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
                // 按实际宽度让位，别写死 330。当前最长的配件名约 306px，
                // 加间距还不到 330，所以现在走的是 330 这一支，与改前一致；
                // 以后加了更长的名字才轮到让位。
                // （差一点点：Selectable 的文字有个 FramePadding 的左缩进没算进来，
                //   真触发的那天会少让约 8px，不影响可读性）
                same_line_after( m.name.c_str(), 330.0f );
                // 上机匣会带来口径，列出来才知道装上去能打什么弹
                const std::string ammo_note =
                    m.ammo_modifier.empty() ? "" : ( "  " + m.ammo_modifier[0] );
                ImGui::TextDisabled( "操控%+.0f  瞄准%+.0f%s", m.handling_modifier,
                                     m.aim_speed_modifier, ammo_note.c_str() );
            }
        }
        if( !any ) {
            ImGui::PushTextWrapPos( 0.0f );
            ImGui::TextDisabled( "%s", zh::g::NO_COMPAT );
            ImGui::PopTextWrapPos();
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    // 表格画完了，现在安全地改数据
    if( remove_which >= 0 ) {
        remove_mod_at( remove_which );
    }
}

// 瞄准参数
void draw_aim_params( const Gun &g, const DetailCache &d )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_AIMPARAM, ImGuiTreeNodeFlags_DefaultOpen ) ) {
        return;
    }

    kv( zh::g::VOL_FACTOR, "%.3f", aim_factor_from_volume( g, effective_volume( g ) ) );
    kv( zh::g::LEN_FACTOR, "%.3f / %.3f",
        aim_factor_from_length( g.longest_side_mm, false ),
        aim_factor_from_length( g.longest_side_mm, true ) );
    kv( zh::g::RECOIL_LIMIT, "%.0f", d.accuracy_limit );
    kv( zh::g::TOTAL_DISP, "%.1f", d.fixed_disp );
    kv( zh::g::ADDED_RECOIL, "%.0f", d.added_recoil );
    note( zh::g::NOTE_ADDED, recoil_absorb( g_ch.skill_level ) * 100.0 );
}

// 游戏内显示值
void draw_game_values( const Gun &g, const Ammo *ammo )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_GAMEVAL, ImGuiTreeNodeFlags_DefaultOpen ) ) {
        return;
    }
    note( "%s", zh::g::GV_HINT );

    const int d_gun  = (int)game_dispersion_gun( g );
    const int d_ammo = (int)game_dispersion_ammo( g, ammo );
    kv( zh::g::GV_DISP, "%d+%d = %d", d_gun, d_ammo, d_gun + d_ammo );
    note( "%s", zh::g::NOTE_DISP );

    // 腰射时（DISABLE_SIGHTS）压根没有瞄具，直接给腰射极限
    const std::pair<int, int> sd = sight_dispersion_pair( g, g_ch );
    const int psl = (int)point_shooting_limit( g_ch.skill( g.skill ), g.skill == "archery" );
    if( psl <= sd.second ) {
        kv( zh::g::GV_SIGHT_PS, "%d", psl );
    } else {
        kv( zh::g::GV_SIGHT, "%d+%d = %d", sd.first, sd.second - sd.first, sd.second );
    }
    note( "%s", zh::g::NOTE_SIGHT );

    kv( zh::g::GV_RECOIL, "%.0f", game_recoil( g, g_ch, ammo ) );
    note( "%s", zh::g::NOTE_RECOIL );
    if( g.has_mod( "underbarrel" ) ) {
        kv( zh::g::GV_BIPOD, "%.0f", game_recoil_bipod( g, g_ch, ammo ) );
    }

    kv( zh::g::GV_MINREC, "%.0f（%s）", game_min_recoil( g, g_ch, ammo ),
        fmt_str( zh::g::GV_STR_REQ, (int)( gun_base_weight( g ) / 333.0 ) ).c_str() );
    note( "%s", zh::g::NOTE_MINREC );
}

// 瞄准等级 —— 对应游戏物品界面里每档列出的两项
void draw_aim_levels( const DetailCache &d )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_AIMLEVEL, ImGuiTreeNodeFlags_DefaultOpen ) ) {
        return;
    }
    note( "%s", zh::g::NOTE_AIMLEVEL );

    if( ImGui::BeginTable( "aimlv", 3,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV ) ) {
        ImGui::TableSetupColumn( zh::g::COL_AIMLEVEL, ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( zh::g::COL_50RANGE, ImGuiTableColumnFlags_WidthFixed, 140 );
        ImGui::TableSetupColumn( zh::g::COL_AIMTIME, ImGuiTableColumnFlags_WidthFixed, 140 );
        ImGui::TableHeadersRow();

        const struct { const char *name; int rng, mv; } rows[] = {
            { zh::AIM_LEVEL_1, d.aim_range[0], d.aim.moves_to_regular },
            { zh::AIM_LEVEL_2, d.aim_range[1], d.aim.moves_to_careful },
            { zh::AIM_LEVEL_3, d.aim_range[2], d.aim.moves_to_precise },
        };
        for( const auto &r : rows ) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted( r.name );
            ImGui::TableNextColumn();
            if( r.rng >= 59 ) ImGui::TextUnformatted( zh::g::OVER_TABLE );
            else              ImGui::Text( zh::g::TILE_FMT, r.rng );
            ImGui::TableNextColumn(); ImGui::Text( zh::g::AP, r.mv );
        }
        ImGui::EndTable();
    }
}

// 瞄准时间线
void draw_aim_timeline( const DetailCache &d )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_TIMELINE ) ) {
        return;
    }
    note( "%s", zh::g::TIMELINE_HINT );

    const struct { const char *name; int mv; } rows[] = {
        { zh::AIM_LEVEL_1, d.aim.moves_to_regular },
        { zh::AIM_LEVEL_2, d.aim.moves_to_careful },
        { zh::AIM_LEVEL_3, d.aim.moves_to_precise },
    };
    for( const auto &r : rows ) {
        ImGui::Bullet();
        const std::string line = fmt_str( zh::g::TO_LEVEL, r.name );
        ImGui::TextUnformatted( line.c_str() );
        same_line_after( line.c_str() );
        ImGui::Text( zh::g::AP, r.mv );
    }

    ImGui::Spacing();
    kv( zh::g::ONE_TURN, "%.0f", d.aim.recoil_after_1_turn );
    note( zh::g::THRESHOLDS, (int)d.aim.regular_th, (int)d.aim.careful_th,
          (int)d.aim.precise_th );

    // 上面那些数字画成图就是这条曲线：纵轴是回合开始时的瞄准误差，
    // 从 3000 一路降到「瞄准精度上限」就平了。三条橙色横线是档位阈值，
    // 曲线穿过哪条，就说明那一回合刚好压进该档位。
    ImGui::Spacing();
    note( "%s", zh::g::TIMELINE_HINT2 );
    const std::vector<ChartMark> marks = {
        { d.aim.regular_th, zh::AIM_LEVEL_1 },
        { d.aim.careful_th, zh::AIM_LEVEL_2 },
        { d.aim.precise_th, zh::AIM_LEVEL_3 },
    };
    draw_line_chart( "##recoil_curve", d.recoil_curve, zh::g::AXIS_RECOIL,
                     "%.0f", zh::g::RECOIL_TIP, marks );
}

// 通用折线图：X 轴固定是「瞄准回合」，Y 轴由调用方给名字和格式。
// data[i] = 第 i 回合的值。marks 是可选的参考横线。
//
// 自己用 ImDrawList 画，没引 ImPlot —— 见文件头「图表是手绘的」那段。
//
// 坐标轴、网格、折线、填充、刻度取整、参考线、悬停提示都在这一个函数里，
// 两个图（瞄准收益曲线、瞄准时间线的误差曲线）共用。
void draw_line_chart( const char *id,
                      const std::vector<double> &data,
                      const char *y_label,
                      const char *y_fmt,
                      const char *tip_fmt,
                      const std::vector<ChartMark> &marks )   // 默认实参见前置声明
{
    const int N = (int)data.size();
    if( N < 2 ) {
        return;
    }

    const float  W      = std::max( 360.0f, ImGui::GetContentRegionAvail().x - 8.0f );
    const float  H      = 280.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton( id, ImVec2( W, H ) );
    const bool   hovered = ImGui::IsItemHovered();
    const ImVec2 mouse   = ImGui::GetIO().MousePos;

    ImDrawList *dl = ImGui::GetWindowDrawList();

    // 绘图区。四周的边距留给坐标轴：左边放 Y 刻度数字，下面放 X 刻度数字，
    // 上面一行放 Y 轴名。B 要够大 —— 刻度数字和轴名是上下两行，留窄了会叠在一起
    const float L = 56.0f, R = 18.0f, T = 36.0f, B = 52.0f;
    const ImVec2 a( origin.x + L,     origin.y + T );
    const ImVec2 b( origin.x + W - R, origin.y + H - B );

    // Y 轴范围要把参考线也框进来，否则阈值线会画到图外面
    double maxY = 1.0;
    for( double v : data ) {
        maxY = std::max( maxY, v );
    }
    for( const ChartMark &m : marks ) {
        maxY = std::max( maxY, m.y );
    }
    const double step = nice_step( maxY, 5 );
    const double top  = std::max( step, std::ceil( maxY / step ) * step );

    auto px = [&]( int t ) {
        return a.x + ( b.x - a.x ) * (float)t / (float)std::max( 1, N - 1 );
    };
    auto py = [&]( double v ) {
        return b.y - ( b.y - a.y ) * (float)( v / top );
    };

    const ImU32 col_bg   = ImGui::GetColorU32( ImGuiCol_FrameBg );
    const ImU32 col_grid = ImGui::GetColorU32( ImGuiCol_Border, 0.7f );
    const ImU32 col_axis = ImGui::GetColorU32( ImGuiCol_Text, 0.45f );
    const ImU32 col_text = ImGui::GetColorU32( ImGuiCol_Text, 0.60f );
    const ImU32 col_line = IM_COL32( 100, 180, 255, 255 );
    const ImU32 col_fill = IM_COL32( 100, 180, 255, 40 );
    const ImU32 col_dot  = IM_COL32( 170, 215, 255, 255 );
    const ImU32 col_mark = IM_COL32( 235, 170, 80, 220 );

    dl->AddRectFilled( a, b, col_bg );

    // 横向网格 + Y 轴刻度
    for( double v = 0.0; v <= top + 1e-9; v += step ) {
        const float y = py( v );
        dl->AddLine( ImVec2( a.x, y ), ImVec2( b.x, y ), col_grid );
        const std::string lab = fmt_str( y_fmt, v );
        const ImVec2 ts = ImGui::CalcTextSize( lab.c_str() );
        dl->AddText( ImVec2( a.x - 8.0f - ts.x, y - ts.y * 0.5f ), col_text, lab.c_str() );
    }

    // 纵向网格 + X 轴刻度（回合多了就隔几个标一个，别挤成一团）
    const int xstep = std::max( 1, ( N - 1 ) / 10 );
    for( int t = 0; t < N; t += xstep ) {
        const float x = px( t );
        dl->AddLine( ImVec2( x, a.y ), ImVec2( x, b.y ), col_grid );
        const std::string lab = fmt_str( "%d", t );
        const ImVec2 ts = ImGui::CalcTextSize( lab.c_str() );
        dl->AddText( ImVec2( x - ts.x * 0.5f, b.y + 6.0f ), col_text, lab.c_str() );
    }

    // 坐标轴
    dl->AddLine( ImVec2( a.x, b.y ), ImVec2( b.x, b.y ), col_axis, 1.5f );
    dl->AddLine( ImVec2( a.x, a.y ), ImVec2( a.x, b.y ), col_axis, 1.5f );

    // 折线 + 下面垫一层半透明填充
    std::vector<ImVec2> pts;
    pts.reserve( N );
    for( int t = 0; t < N; t++ ) {
        pts.emplace_back( px( t ), py( data[t] ) );
    }
    for( int t = 0; t + 1 < N; t++ ) {
        dl->AddQuadFilled( pts[t], pts[t + 1],
                           ImVec2( pts[t + 1].x, b.y ), ImVec2( pts[t].x, b.y ), col_fill );
    }
    dl->AddPolyline( pts.data(), N, col_line, ImDrawFlags_None, 2.0f );
    for( const ImVec2 &p : pts ) {
        dl->AddCircleFilled( p, 3.0f, col_dot );
    }

    // 参考横线。标签不画在线上 —— 阈值通常远小于纵轴上限（比如 348 / 127 / 54
    // 对 3000），三条线全挤在最下面一截，各自带标签会叠成一团。改成右上角的
    // 图例：曲线从左上降到右下，右上角一定是空的，压不到数据。
    for( const ChartMark &m : marks ) {
        const float y = py( m.y );
        if( y < a.y - 1.0f || y > b.y + 1.0f ) {
            continue;
        }
        dl->AddLine( ImVec2( a.x, y ), ImVec2( b.x, y ), col_mark, 1.5f );
    }

    if( !marks.empty() ) {
        // 图例里带数值，省得再去对上面那行「档位阈值：…」的文字。
        // ★ 用 (int) 截断而不是 %.0f —— 阈值是小数（比如 348.6），
        //   四舍五入会显示成 349，和上面那行（用截断）差一个数，看着像 bug。
        std::vector<std::string> entries;
        float lw = 0.0f;
        for( const ChartMark &m : marks ) {
            entries.push_back( fmt_str( zh::g::LEGEND_FMT, m.label, (int)m.y ) );
            lw = std::max( lw, ImGui::CalcTextSize( entries.back().c_str() ).x );
        }

        const float lineH = ImGui::GetTextLineHeight();
        const float pad   = 6.0f, swatch = 16.0f;
        const ImVec2 p0( b.x - lw - swatch - pad * 3.0f, a.y + pad );
        const ImVec2 p1( b.x - pad, p0.y + lineH * (float)entries.size() + pad * 2.0f );

        dl->AddRectFilled( p0, p1, IM_COL32( 16, 16, 20, 215 ), 3.0f );
        dl->AddRect( p0, p1, col_grid, 3.0f );

        float ty = p0.y + pad;
        for( const std::string &e : entries ) {
            const float cy = ty + lineH * 0.5f;
            dl->AddLine( ImVec2( p0.x + pad, cy ), ImVec2( p0.x + pad + swatch, cy ),
                         col_mark, 2.0f );
            dl->AddText( ImVec2( p0.x + pad + swatch + pad, ty ), col_mark, e.c_str() );
            ty += lineH;
        }
    }

    // 悬停：竖线 + 该回合的数值
    if( hovered && mouse.x >= a.x - 6.0f && mouse.x <= b.x + 6.0f ) {
        int t = (int)std::lround( ( mouse.x - a.x ) / ( b.x - a.x )
                                  * (float)std::max( 1, N - 1 ) );
        t = std::max( 0, std::min( N - 1, t ) );
        const ImVec2 p = pts[t];
        dl->AddLine( ImVec2( p.x, a.y ), ImVec2( p.x, b.y ),
                     ImGui::GetColorU32( ImGuiCol_Text, 0.3f ) );
        dl->AddCircleFilled( p, 5.0f, col_line );

        ImGui::BeginTooltip();
        ImGui::TextUnformatted( fmt_str( tip_fmt, t, data[t] ).c_str() );
        ImGui::EndTooltip();
    }

    // 轴名。这两个是字面量，直接画 —— 别丢进 printf，里面那个 % 会被当格式符。
    // Y 轴名放绘图区左上角（竖排太麻烦），X 轴名居中放在刻度下面
    dl->AddText( ImVec2( a.x, origin.y + 6.0f ), col_text, y_label );
    {
        const ImVec2 ts = ImGui::CalcTextSize( zh::g::AXIS_TURN );
        dl->AddText( ImVec2( ( a.x + b.x ) * 0.5f - ts.x * 0.5f, b.y + 28.0f ),
                     col_text, zh::g::AXIS_TURN );
    }
}

// 瞄准收益曲线：横轴 = 瞄准回合，纵轴 = 50%好击距离
void draw_range_curve( const DetailCache &d )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_CURVE, ImGuiTreeNodeFlags_DefaultOpen ) ) {
        return;
    }
    note( "%s", zh::g::CURVE_HINT );
    draw_line_chart( "##range_curve", d.curve, zh::g::AXIS_RANGE, "%.0f", zh::g::CURVE_TIP );
}

// 瞄准档位实例
void draw_instance( const DetailCache &d )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_INSTANCE ) ) {
        return;
    }
    note( "%s", zh::g::INSTANCE_HINT );

    if( ImGui::BeginTable( "instance", 5,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV ) ) {
        ImGui::TableSetupColumn( zh::g::COL_AIMLEVEL, ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( zh::g::COL_RECOIL, ImGuiTableColumnFlags_WidthFixed, 100 );
        ImGui::TableSetupColumn( zh::g::COL_FIXDISP, ImGuiTableColumnFlags_WidthFixed, 100 );
        ImGui::TableSetupColumn( zh::g::COL_TOTDISP, ImGuiTableColumnFlags_WidthFixed, 100 );
        ImGui::TableSetupColumn( zh::g::COL_50RANGE, ImGuiTableColumnFlags_WidthFixed, 140 );
        ImGui::TableHeadersRow();

        // 与命令行版一致：完全没瞄 = 最大后坐；另外三档 = 按瞄准进度插值
        const double limit = d.accuracy_limit;
        const struct { const char *name; double recoil; } rows[] = {
            { zh::AIM_LEVEL_0, MAX_RECOIL },
            { zh::AIM_LEVEL_1, ( ( MAX_RECOIL - limit ) / 10.0 ) + limit },
            { zh::AIM_LEVEL_2, ( ( MAX_RECOIL - limit ) / 40.0 ) + limit },
            { zh::AIM_LEVEL_3, limit },
        };
        for( const auto &r : rows ) {
            const double total = d.fixed_disp + r.recoil;
            const int rng = range_with_even_chance_of_good_hit( total );
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted( r.name );
            ImGui::TableNextColumn(); ImGui::Text( "%.0f", r.recoil );
            ImGui::TableNextColumn(); ImGui::Text( "%.0f", d.fixed_disp );
            ImGui::TableNextColumn(); ImGui::Text( "%.0f", total );
            ImGui::TableNextColumn();
            if( rng >= 59 ) ImGui::TextUnformatted( zh::g::OVER_TABLE );
            else            ImGui::Text( zh::g::TILE_FMT, rng );
        }
        ImGui::EndTable();
    }
}

// 命中档位概率（贵，只在展开时算）
void draw_probabilities( const Gun &g, const Ammo *ammo )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_PROB, ImGuiTreeNodeFlags_DefaultOpen ) ) {
        return;                       // 没展开就一分钱不花
    }
    note( "%s", zh::g::PROB_HINT );
    note( "%s", zh::g::PROB_RULE );

    if( !key_matches( g_probs ) ) {
        compute_probs( g, ammo, g_detail.aim );
    }

    const char *lv_name[PROB_NLEVEL] = {
        zh::AIM_LEVEL_0, zh::AIM_LEVEL_1, zh::AIM_LEVEL_2, zh::AIM_LEVEL_3
    };

    for( int L = 0; L < PROB_NLEVEL; L++ ) {
        ImGui::Spacing();
        ImGui::Text( zh::g::PROB_LEVEL, lv_name[L], (int)g_probs.th[L] );

        ImGui::PushID( L );
        if( ImGui::BeginTable( "prob", PROB_NTIER + 1,
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV ) ) {
            ImGui::TableSetupColumn( zh::g::COL_DIST, ImGuiTableColumnFlags_WidthFixed, 90 );
            for( int t = 0; t < PROB_NTIER; t++ ) {
                ImGui::TableSetupColumn( zh::hit_tier_short( t ),
                                         ImGuiTableColumnFlags_WidthStretch );
            }
            ImGui::TableHeadersRow();

            for( int d = 0; d < PROB_NDIST; d++ ) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text( zh::g::TILE_FMT, (int)PROB_DISTS[d] );
                for( int t = 0; t < PROB_NTIER; t++ ) {
                    ImGui::TableNextColumn();
                    ImGui::Text( "%.1f%%", g_probs.pct[L][d][t] );
                }
            }
            ImGui::EndTable();
        }
        ImGui::PopID();
    }
}

void draw_detail()
{
    ImGui::BeginChild( "detail", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
    if( g_selected < 0 ) {
        ImGui::TextDisabled( "%s", zh::g::PICK_GUN );
        ImGui::EndChild();
        return;
    }

    const Gun  &g    = g_work;        // 工作副本：含用户装的配件
    const Ammo *ammo = current_ammo();

    draw_detail_header( g );
    if( ammo == nullptr ) {
        // 模块化枪械没装机匣时没有口径，但配件还是要能装 —— 装备区不能藏
        ImGui::Spacing();
        draw_mods( g );
        ImGui::EndChild();
        return;
    }

    if( !key_matches( g_detail ) ) {
        compute_detail( g, ammo );
    }

    ImGui::Spacing();
    draw_mods( g );
    draw_aim_params( g, g_detail );
    draw_game_values( g, ammo );
    draw_aim_levels( g_detail );
    draw_aim_timeline( g_detail );
    draw_range_curve( g_detail );
    draw_instance( g_detail );
    draw_probabilities( g, ammo );

    ImGui::EndChild();
}

void draw_ui()
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos( vp->WorkPos );
    ImGui::SetNextWindowSize( vp->WorkSize );
    ImGui::Begin( "gunlab", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoSavedSettings );

    draw_toolbar();
    ImGui::Separator();

    draw_gun_list();
    ImGui::SameLine();
    draw_detail();

    ImGui::End();
}

} // namespace

// =============================================================================
//  主程序
// =============================================================================

// 启动阶段失败时的报错。
//
// ★ 这个 exe 是 GUI 子系统（CMakeLists 里设了 WIN32_EXECUTABLE），没有控制台，
//   所以 printf 给双击启动的用户看是白搭 —— 必须弹消息框，否则现象就是
//   「双击了，什么都没发生」。printf 留着，是为了重定向到文件时还能收到。
[[noreturn]] void fatal( const char *what )
{
    const std::string msg = fmt_str( "%s：%s", what, SDL_GetError() );
    std::printf( "%s\n", msg.c_str() );
    SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "gunlab 启动失败",
                              msg.c_str(), nullptr );
    std::exit( 1 );
}

int main( int argc, char **argv )
{
    // 没有控制台，stdout 只在你主动重定向时才有去处。重定向到文件/管道时
    // 默认是「全缓冲」，进程被强杀就一个字都留不下 —— 下面那两条启动诊断
    // （字体路径、窗口尺寸）是排查界面问题的第一手线索，必须随打随见。
    std::setvbuf( stdout, nullptr, _IONBF, 0 );

    init_database();
    build_skill_index();

    if( argc > 1 ) {
        std::snprintf( g_search, sizeof( g_search ), "%s", argv[1] );
    }
    refresh_hits();
    if( !g_hits.empty() ) {
        select_gun( g_hits[0] );
    }

    if( !SDL_Init( SDL_INIT_VIDEO ) ) {
        fatal( "SDL 初始化失败" );
    }

    SDL_Window *win = SDL_CreateWindow( "gunlab — Cataclysm 枪械计算器",
                                        1400, 900,
                                        SDL_WINDOW_RESIZABLE |
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY );
    if( win == nullptr ) {
        fatal( "创建窗口失败" );
    }

    // 显式显示并提到前台（虽然 SDL3 默认就会显示，但某些环境下不会自动置顶）
    SDL_ShowWindow( win );
    SDL_RaiseWindow( win );

    // 启动时把尺寸打出来。高 DPI 缩放下「窗口逻辑尺寸」和「实际像素尺寸」可能
    // 不一致，一旦 ImGui 的布局尺寸大于 framebuffer，界面右边就会被切掉 ——
    // 这一行能立刻看出是哪一边不对。
    {
        int lw = 0, lh = 0, pw = 0, ph = 0;
        SDL_GetWindowSize( win, &lw, &lh );
        SDL_GetWindowSizeInPixels( win, &pw, &ph );
        const SDL_DisplayID disp = SDL_GetPrimaryDisplay();
        SDL_Rect ub{ 0, 0, 0, 0 };
        SDL_GetDisplayUsableBounds( disp, &ub );
        std::printf( "窗口 %dx%d 逻辑 / %dx%d 像素；屏幕可用区 %dx%d，缩放 %.3f\n",
                     lw, lh, pw, ph, ub.w, ub.h,
                     (double)SDL_GetDisplayContentScale( disp ) );
    }

    SDL_Renderer *ren = SDL_CreateRenderer( win, nullptr );
    if( ren == nullptr ) {
        // 窗口已经建出来了，报错框还能挂在它上面（不挂就是独立的一个框）
        const std::string msg = fmt_str( "创建渲染器失败：%s", SDL_GetError() );
        std::printf( "%s\n", msg.c_str() );
        SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "gunlab 启动失败",
                                  msg.c_str(), win );
        SDL_DestroyWindow( win );
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;          // 不写 imgui.ini，保持目录干净

    load_cjk_font( io, 18.0f );
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer( win, ren );
    ImGui_ImplSDLRenderer3_Init( ren );

    bool running = true;
    while( running ) {
        SDL_Event e;
        while( SDL_PollEvent( &e ) ) {
            ImGui_ImplSDL3_ProcessEvent( &e );
            if( e.type == SDL_EVENT_QUIT ) {
                running = false;
            }
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        draw_ui();

        ImGui::Render();
        SDL_SetRenderDrawColor( ren, 18, 18, 22, 255 );
        SDL_RenderClear( ren );
        ImGui_ImplSDLRenderer3_RenderDrawData( ImGui::GetDrawData(), ren );
        SDL_RenderPresent( ren );
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer( ren );
    SDL_DestroyWindow( win );
    SDL_Quit();
    return 0;
}
