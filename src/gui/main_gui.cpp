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
#include <map>
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

// 曲线横轴（瞄准回合）的最大值。多条曲线共用一个横轴，所以是定长。
constexpr int CURVE_MAX_TURNS = 8;

// 所有数据表共用的标志。
//
// Resizable 让列宽可以拖 —— 这里的内容长短差异很大（枪名、配件名、
// 「好击及以上 @10格」这种长表头），写死列宽总有被挤的时候。
// 拖动要配 BordersInnerV 才有可抓的分隔线。
constexpr ImGuiTableFlags TABLE_FLAGS =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

// ---- 左右分栏 ---------------------------------------------------------------
// 列表宽度可拖。上下限的意义：列表窄到 200 就看不清枪名了；
// 详情面板窄到 360 那些「标签 —— 数值」的行会折行。
float g_list_width = 380.0f;
constexpr float LIST_MIN_W   = 200.0f;
constexpr float DETAIL_MIN_W = 360.0f;
constexpr float SPLITTER_W   = 8.0f;

// 逐发明细表最多列几发（游戏里有 50 发、100 发的高速全自动）
constexpr int BURST_MAX_ROWS = 12;
// 逐发明细里每发算概率用的采样数。概率表那边用 20 万，这里最多 12 发，
// 乘起来太贵；2 万够看出量级了（实测整套约 30ms，只在缓存失效时跑一次）。
constexpr int    BURST_PROB_N   = 20000;
constexpr double BURST_REF_DIST = 10.0;    // 「好击及以上」按 10 格外算（列头里有写）

// 逐发明细用哪个「两发之间的瞄准回合数」。做成可调是为了让它和曲线图的
// 横轴对上 —— 图上看中哪个位置，就把间隔调过去看那一轮的逐发明细。
int g_burst_interval = 2;

// 「误差 → 命中档位」那张图按多少格算。命中档位不只由误差决定，还跟距离有关
// （未命中度 = 横向偏移 ÷ 目标体积，偏移随距离线性放大），所以距离必须是个参数。
int g_tier_dist = 10;

// 那张图的采样点（瞄准误差）。刻意不等距：概率的变化几乎都发生在低误差段，
// 3000 那一带早就平了。等距采样的话右半张图纯属浪费。
const double TIER_ERR_STEPS[] = {
    0, 25, 50, 75, 100, 125, 150, 200, 250, 300, 350, 400,
    500, 600, 700, 850, 1000, 1200, 1500, 2000, 2500, 3000
};
constexpr int TIER_ERR_N = (int)( sizeof( TIER_ERR_STEPS ) / sizeof( TIER_ERR_STEPS[0] ) );
// 每点采样多少次。22 个点 × 2 万 = 44 万次掷骰，实测约 60ms，
// 只在缓存失效时跑一次（和逐发明细那次是一个量级）。
constexpr int TIER_PROB_N = 20000;

// 六个档位的颜色，和概率表里的列一一对应
constexpr ImU32 TIER_COLORS[6] = {
    IM_COL32( 240, 105, 105, 255 ),   // 爆头
    IM_COL32( 240, 165,  90, 255 ),   // 暴击
    IM_COL32( 205, 220, 100, 255 ),   // 好击
    IM_COL32( 120, 215, 140, 255 ),   // 普通
    IM_COL32( 110, 175, 235, 255 ),   // 擦伤
    IM_COL32( 150, 150, 160, 255 ),   // 脱靶
};

// 逐发明细表里用来标点的颜色（曲线图用不到，那儿每个模式一个色）
constexpr ImU32 COLOR_FIRST = IM_COL32( 120, 175, 235, 255 );   // 首次开火（蓝）
constexpr ImU32 COLOR_SUST_1 = IM_COL32( 120, 215, 140, 255 );  // 第一个持续模式（绿）
constexpr ImU32 COLOR_SUST_2 = IM_COL32( 240, 165, 90, 255 );   // 第二个（橙）
constexpr ImU32 COLOR_SUST_3 = IM_COL32( 215, 130, 210, 255 );  // 第三个（紫）

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

// 工具栏第二行（人物参数）可以收起来，把纵向空间让给下面的列表和详情。
// 小窗口下多一行就能多看两把枪。收起时旁边显示一份紧凑摘要，
// 免得不知道当前参数是什么。
bool g_show_char_params = true;

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

    // 连射：每个射击模式一条「持续射击」曲线（长度与 curve 相同），
    // 外加稳态下这一轮逐发的明细。
    struct SustainedCurve {
        std::string         label;                 // 图例名
        int                 qty = 1;
        bool                reload_and_shoot = false;
        std::vector<double> range;                 // range[t] = 间隔 t 回合时的 50%好击距离

        // 逐发明细，取的是 burst_interval 那个间隔下的稳态。
        // 只对 qty > 1 的模式算 —— 一轮一发的没什么好拆的。
        double              burst_start_recoil = 0.0;
        std::vector<double> burst_shot_recoil;
        std::vector<double> burst_goodplus;        // 每发「好击及以上」的概率（0~1）
        double              burst_end_recoil = 0.0;
        int                 burst_shown = 0;       // 实际列出的发数（可能被截断）
        bool                burst_over_cap = false; // 中途有没有超过 MAX_RECOIL
    };
    std::vector<SustainedCurve> sustained;
    int burst_interval = 2;                        // 逐发明细用哪个间隔（见 g_burst_interval）

    // 瞄准误差 → 各命中档位的概率分布。
    //   tier_err[i]     = 第 i 个采样点的瞄准误差（横轴刻度就显示它）
    //   tier_pct[t][i]  = 该误差下、落在档位 t 的概率（0~1）
    // 采样点在低误差段密、高误差段疏 —— 概率变化几乎都发生在低段，
    // 等距采样的话右半张图是一马平川。
    std::vector<double>              tier_err;
    std::vector<std::vector<double>> tier_pct;     // [档位][采样点]
    int tier_dist = 10;                            // 按多少格算的（见 g_tier_dist）
};

// 目标体积固定 1.0 格（人形怪）。命中档位还跟目标体积有关，本工具不做区分。
constexpr double PROB_TARGET = 1.0;
// 固定随机种子 —— 结果可复现，反复对照同一个数不会变
constexpr unsigned PROB_SEED = 20260910u;

DetailCache g_detail;

// ---- 小工具 -----------------------------------------------------------------

// 让编译器按 printf 规则检查自定义的变参函数。
//
// ★ 别指望它兜住所有情况：GCC 的 -Wformat **只检查字符串字面量**，而本项目
//   绝大多数格式串是 zh::g::XXX 这样的常量变量，它取不到值就跳过不查。
//   （实测过：故意把 %d 写成 %.0f 再传 int，加了属性照样 0 警告。）
//   所以这里只能拦住少数直接传字面量的调用，剩下的得自己盯。
//
//   真踩过：卡壳警告里写 %.0f 却传 (int)，界面上显示成「recoil 0」。
//   varargs 里 int 和 double 对不上是未定义行为 —— 不报错，但值是错的。
#if defined(__GNUC__)
#  define PRINTF_LIKE( fmt_idx, arg_idx ) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#  define PRINTF_LIKE( fmt_idx, arg_idx )
#endif

// 界面上的文案大多带 %d / %.1f 之类的占位符，先格式化再交给 ImGui。
// 不用 std::format 是因为 GCC 16 的 libstdc++ 才有，MSVC 侧版本不一致。
std::string fmt_str( const char *fmt, ... ) PRINTF_LIKE( 1, 2 );
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
void kv( const char *label, const char *fmt, ... ) PRINTF_LIKE( 2, 3 );
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

// 每张图各自的可调状态：横向缩放、视野中心、高度。
// 用图的 id 当键 —— 三张图共用一个 draw_line_chart，但状态要各管各的。
struct ChartView {
    float zoom   = 1.0f;    // 1 = 看全部；>1 = 放大
    float cx     = 0.5f;    // 视野中心，归一化到数据下标（0~1）
    float height = 0.0f;    // 0 = 用 ChartOpts::height 的默认值，拖过之后才有效
};
std::map<std::string, ChartView> g_chart_views;

// 图上的一条线
struct ChartSeries {
    std::string         name;    // 图例名，空字符串 = 不上图例（单线图用）
    std::vector<double> data;
    ImU32               color = 0;
};

// 折线图的参数。
// 收成结构体是因为字段一多，位置传参就很容易串 —— 尤其 y_ticks(int) 和
// height(float) 挨着，传反了会隐式转换、编译都不报错。
struct ChartOpts {
    const char *y_label;                             // 纵轴名，画在左上角，也用作单线图的悬停标签
    const char *x_label = zh::g::AXIS_TURN;          // 横轴名，画在刻度下面
    const char *y_unit = "";                         // 值的单位后缀，如 " 格"
    const char *x_unit = "";                         // 横轴刻度值的单位后缀（悬停提示用）
    const char *y_fmt  = "%.0f";                     // 纵轴刻度格式
    // 自定义横轴刻度文字。**每个点都要填**（悬停提示要用它显示横轴的值），
    // 留空则用下标（0/1/2…）。用在「横轴不是回合」的图上 —— 比如误差那张图，
    // 刻度要显示误差值本身，而且采样点是非等距的（低误差段密、高误差段疏）。
    std::vector<std::string> x_labels;
    // 轴上每隔几个点画一个刻度文字。0 = 自动（按点数估一个不挤的值）。
    // ★ 只影响画多少，不影响 x_labels 的内容 —— 提示里要的是那个点的准确值，
    //   不是「轴上画没画」。
    int x_tick_step = 0;
    const std::vector<ChartMark> *marks = nullptr;   // 参考横线，可空
    int         y_ticks = 5;                         // 想要几格，实际步长取整成 1/2/5×10ⁿ
    float       height  = 280.0f;                    // 绘图区总高（像素）
};

// 定义在下面「瞄准档位实例」附近。这里前置声明是因为瞄准时间线的图要用它，
// 而那个函数定义在更前面。
void draw_line_chart( const char *id, const std::vector<ChartSeries> &series,
                      const ChartOpts &opts );

// 灰色小字说明，自动换行
void note( const char *fmt, ... ) PRINTF_LIKE( 1, 2 );
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

// DetailCache 的键里还多两项计算输入：逐发明细的间隔、误差图的距离
bool key_matches_detail( const DetailCache &c )
{
    return key_matches( c )
        && c.burst_interval == g_burst_interval
        && c.tier_dist == g_tier_dist;
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
        // 被去重掉的档位阈值是 -1，算出来没意义，直接置 0
        g_detail.aim_range[i] = ( th[i] < 0.0 )
            ? 0
            : range_with_even_chance_of_good_hit( g_detail.fixed_disp + th[i] );
    }

    // 后面的曲线和连射计算都要用这个 ctx，算一次共用
    AimContext cctx;
    cctx.len_factor = 1.0;
    cctx.limit      = g_detail.accuracy_limit;
    cctx.vol_factor = aim_factor_from_volume( g, effective_volume( g ) );

    auto to_range = [&]( double recoil ) {
        return (double)range_with_even_chance_of_good_hit( g_detail.fixed_disp + recoil );
    };

    // ---- 冷启动曲线 ---------------------------------------------------------
    // 横轴第 t 点 = 从满误差瞄 t 回合后开火能打多远。这就是图上的「首次开火」。
    //
    // 注：原来这里「连续 3 回合没变化就提前收尾」（那是给终端 ASCII 图控宽度
    // 用的）。现在多条曲线要共用同一个横轴，改成固定长度。
    g_detail.curve.clear();
    g_detail.recoil_curve.clear();
    {
        double recoil = MAX_RECOIL;
        for( int t = 0; t <= CURVE_MAX_TURNS; t++ ) {
            g_detail.curve.push_back( to_range( recoil ) );
            g_detail.recoil_curve.push_back( recoil );
            recoil = aim_for_turns( g, g_ch, recoil, 1, cctx );
        }
    }

    // ---- 持续射击：每个射击模式一条 -----------------------------------------
    g_detail.burst_interval = g_burst_interval;
    g_detail.sustained.clear();
    for( const GunMode &m : g.modes ) {
        DetailCache::SustainedCurve sc;
        sc.qty  = m.qty;
        sc.reload_and_shoot = g.reload_and_shoot;
        const std::string mname = zh::mode_name( m.name );
        sc.label = ( m.qty == 1 )
                   ? fmt_str( zh::g::MODE_FMT_1, mname.c_str() )
                   : fmt_str( zh::g::MODE_FMT_N, mname.c_str(), m.qty );

        sc.range.reserve( CURVE_MAX_TURNS + 1 );
        for( int t = 0; t <= CURVE_MAX_TURNS; t++ ) {
            sc.range.push_back(
                to_range( sustained_fire_recoil( g, g_ch, ammo, m.qty, t, cctx ) ) );
        }

        // 逐发明细：只对连发模式算（一轮一发的没什么好拆的）。
        // 最多列 12 发 —— 游戏里有 50 发、100 发的高速全自动，全列会刷屏。
        if( m.qty > 1 ) {
            const double start = sustained_fire_recoil(
                g, g_ch, ammo, m.qty, g_detail.burst_interval, cctx );
            const int shown = std::min( m.qty, BURST_MAX_ROWS );
            const BurstResult b = fire_burst( g, g_ch, ammo, start, shown );
            sc.burst_start_recoil = start;
            sc.burst_shot_recoil  = b.shot_recoil;
            sc.burst_end_recoil   = b.recoil_after;
            sc.burst_shown        = shown;

            for( double rec : b.shot_recoil ) {
                if( rec > MAX_RECOIL ) {
                    sc.burst_over_cap = true;
                }
            }

            // 每发「好击及以上」的概率。
            //
            // ★ 放在这里（缓存里算一次）而不是绘图函数里 —— 绘图每帧都跑，
            //   而每发要采样。虽然 N 比概率表小得多（表里 20 万），但乘上
            //   最多 12 发还是会卡。实测 20 万次采样约 25ms。
            std::mt19937 rng( PROB_SEED );
            std::vector<double> samples( BURST_PROB_N );
            sc.burst_goodplus.reserve( b.shot_recoil.size() );
            for( double rec : b.shot_recoil ) {
                for( int k = 0; k < BURST_PROB_N; k++ ) {
                    samples[k] = roll_dispersion( g, g_ch, ammo, rec, rng );
                }
                long good = 0;
                for( int k = 0; k < BURST_PROB_N; k++ ) {
                    if( missed_by( samples[k], BURST_REF_DIST, PROB_TARGET ) < ACC_GOODHIT ) {
                        good++;
                    }
                }
                sc.burst_goodplus.push_back( (double)good / BURST_PROB_N );
            }
        }
        g_detail.sustained.push_back( std::move( sc ) );
    }

    // ---- 瞄准误差 → 各命中档位概率 ------------------------------------------
    // 对每个采样误差掷一批骰，再按距离换算成未命中度、分桶统计。
    // 掷骰本身与距离无关，所以每个误差点只掷一次、六个档位复用同一批样本。
    g_detail.tier_dist = g_tier_dist;
    g_detail.tier_err.assign( TIER_ERR_STEPS, TIER_ERR_STEPS + TIER_ERR_N );
    g_detail.tier_pct.assign( 6, std::vector<double>( TIER_ERR_N, 0.0 ) );
    {
        std::mt19937 rng( PROB_SEED );
        std::vector<double> samples( TIER_PROB_N );
        for( int i = 0; i < TIER_ERR_N; i++ ) {
            for( int k = 0; k < TIER_PROB_N; k++ ) {
                samples[k] = roll_dispersion( g, g_ch, ammo, TIER_ERR_STEPS[i], rng );
            }
            // 统计时**不直接用 tier_index** —— 那是纯按阈值分档，把「爆头」和
            // 「暴击」当成了只要未命中度够低就必然发生。游戏里不是这样：
            //
            //   爆头档：要 goodhit < 0.1 **且**（命中头部 或 fatal_hit）。
            //     命中头部要目标部位数据；fatal_hit = 伤害×暴击倍率 > 目标血量，
            //     要伤害和血量。两样 gunlab 都没建模 —— 而且玩家作目标时那条
            //     部位图路径在 goodhit < 0.1 下**永远选不中头**（躯干 36 的
            //     权重把 value 全吃掉了），所以这一档实际主要来自 fatal_hit，
            //     也就是「一枪能打死」。它依赖目标，本工具给不了。
            //   暴击档：要 goodhit < 0.2 **且**过 crit_roll。crit_roll 可以算：
            //     hit_roll ~ U(goodhit, 1)，判定是 hit_roll*0.5 < 0.2
            //     即 hit_roll < 0.4，所以 P = (0.4 − g) / (1 − g)。
            //     没过的那部分会**掉回好击**（代码是 else-if 链）。
            //
            // 所以这里统计的是一个「真实分布」：爆头恒为 0（给不出来），
            // 暴击是过了 crit_roll 的真实比例，好击把没过的那些收进来。
            double acc[6] = { 0, 0, 0, 0, 0, 0 };
            for( int k = 0; k < TIER_PROB_N; k++ ) {
                const double g = missed_by( samples[k], (double)g_tier_dist,
                                            PROB_TARGET );
                if( g >= ACC_GRAZING ) {
                    acc[5] += 1.0;
                } else if( g >= ACC_STANDARD ) {
                    acc[4] += 1.0;
                } else if( g >= ACC_GOODHIT ) {
                    acc[3] += 1.0;
                } else if( g >= ACC_CRITICAL ) {
                    acc[2] += 1.0;                       // 好击：不看额外判定
                } else {
                    const double p_crit = ( 0.4 - g ) / ( 1.0 - g );
                    acc[1] += p_crit;                    // 暴击：过了 crit_roll
                    acc[2] += 1.0 - p_crit;              // 没过的掉回好击
                }
            }
            for( int t = 0; t < 6; t++ ) {
                g_detail.tier_pct[t][i] = acc[t] / TIER_PROB_N;
            }
        }
    }

    store_key( g_detail );
    g_detail.valid = true;
}

// compute_probs() 已删除：它算的是「命中档位概率」表。
// 那张表被「误差与命中档位」图取代了 —— 图能连续看误差、距离任意调，
// 而且暴击给的是过了 crit_roll 的真实比例（表里是档位上限）。
// 表里唯一多的是「爆头」，但那一档依赖目标数据，本来也算不出来。

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

    // 折叠开关。放在第一行末尾（而不是第二行行首）—— 收起之后它必须还在，
    // 不然就没法再展开了。
    ImGui::SameLine( 0, GAP * 2 );
    if( ImGui::SmallButton( g_show_char_params ? zh::g::PARAMS_HIDE
                                               : zh::g::PARAMS_SHOW ) ) {
        g_show_char_params = !g_show_char_params;
    }

    if( !g_show_char_params ) {
        // 收起时给一份紧凑摘要，否则看不出当前人物参数是多少
        ImGui::SameLine( 0, GAP * 2 );
        ImGui::TextDisabled( zh::g::PARAMS_SUMMARY_FMT, g_p_dex, g_p_per, g_p_str,
                             zh::skill( g_selected >= 0 ? g_work.skill : "rifle" ).c_str(),
                             g_p_skill, g_p_marks );
        sync_character();
        if( g_selected >= 0 ) {
            g_ch.gun_skill = g_work.skill;
        }
        return;
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

// 列表与详情之间的可拖动分隔条。
//
// ImGui 没有现成的 splitter 控件（`SplitterBehavior` 在 imgui_internal.h 里，
// 是内部 API，不用）。手写其实就三步：一个透明的 InvisibleButton 接住鼠标，
// 拖动时按鼠标位移改宽度，再自己画一条线。
// full_w 是分栏那一行的**总宽**（列表 + 分隔条 + 详情）。
//
// ★ 别在这里调 GetContentRegionAvail() —— 此时列表已经画完，拿到的是「剩下的」
//   宽度，于是上限写成 `上限 = 剩余 − 360`，而剩余又随当前宽度变，成了循环依赖：
//     w = (总宽 − w − 8) − 360  →  w ≈ 总宽的一半
//   结果往右拖只能拖到一半就顶住了。必须由调用方把总宽传进来。
void draw_splitter( float full_w )
{
    // ★ 高度要在 SameLine **之后**取。之前取的话光标还在列表下方（已经到底），
    //   avail 是负数（实测 -4）；ImGui 碰巧把负尺寸当「填满剩余空间」处理了，
    //   结果虽然对，但纯属巧合。SameLine 之后光标回到行首，拿到的是整个高度。
    ImGui::SameLine( 0.0f, 0.0f );
    const float h = ImGui::GetContentRegionAvail().y;
    ImGui::InvisibleButton( "##vsplit", ImVec2( SPLITTER_W, h ) );

    const bool active  = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    if( active ) {
        g_list_width += ImGui::GetIO().MouseDelta.x;
    }
    // 夹在 [最小列表宽, 总宽 − 分隔条 − 最小详情宽] 之间
    const float max_w = std::max( LIST_MIN_W, full_w - SPLITTER_W - DETAIL_MIN_W );
    g_list_width = std::max( LIST_MIN_W, std::min( g_list_width, max_w ) );

    if( active || hovered ) {
        ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );
    }

    // 视觉：一条竖线，悬停变亮、拖动变粗
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    const float  x  = ( p0.x + p1.x ) * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2( x, p0.y ), ImVec2( x, p1.y ),
        ImGui::GetColorU32( active  ? ImGuiCol_SeparatorActive
                          : hovered ? ImGuiCol_SeparatorHovered
                                    : ImGuiCol_Separator ),
        active ? 3.0f : 1.0f );

    ImGui::SameLine( 0.0f, 0.0f );
}

void draw_gun_list()
{
    ImGui::BeginChild( "list", ImVec2( g_list_width, 0 ), ImGuiChildFlags_Borders );
    if( ImGui::BeginTable( "guns", 3,
                           TABLE_FLAGS |
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
                                  TABLE_FLAGS )) {
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
                           TABLE_FLAGS )) {
        ImGui::TableSetupColumn( zh::g::COL_AIMLEVEL, ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( zh::g::COL_50RANGE, ImGuiTableColumnFlags_WidthFixed, 140 );
        ImGui::TableSetupColumn( zh::g::COL_AIMTIME, ImGuiTableColumnFlags_WidthFixed, 140 );
        ImGui::TableHeadersRow();

        // 阈值被去重掉的档位不存在，不列那一行（高散布武器可能只剩一两档）
        const struct { const char *name; int rng, mv; bool exists; } rows[] = {
            { zh::AIM_LEVEL_1, d.aim_range[0], d.aim.moves_to_regular, true },
            { zh::AIM_LEVEL_2, d.aim_range[1], d.aim.moves_to_careful, d.aim.has_careful },
            { zh::AIM_LEVEL_3, d.aim_range[2], d.aim.moves_to_precise, d.aim.has_precise },
        };
        for( const auto &r : rows ) {
            if( !r.exists ) continue;
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
//
// 这里原本还列了三行「压到X档 N 行动点」、一行「一回合后降到 N」和一行
// 档位阈值。现在全删了 —— 前两组数字与上面「瞄准等级」表里的「瞄准用时」
// 完全重复，档位阈值则已经进了图的图例，留着只是把同一组数字说三遍。
// 曲线的走向、每档的穿越点、压到极限后变平，图上都直接看得出来。
void draw_aim_timeline( const DetailCache &d )
{
    // 默认收起：图有 560px 高，展开着会把下面两栏顶得很远
    if( !ImGui::CollapsingHeader( zh::g::SEC_TIMELINE ) ) {
        return;
    }
    note( "%s", zh::g::TIMELINE_HINT2 );
    // 被去重掉的档位阈值是 -1，不画那条参考线
    std::vector<ChartMark> marks = { { d.aim.regular_th, zh::AIM_LEVEL_1 } };
    if( d.aim.has_careful ) { marks.push_back( { d.aim.careful_th, zh::AIM_LEVEL_2 } ); }
    if( d.aim.has_precise ) { marks.push_back( { d.aim.precise_th, zh::AIM_LEVEL_3 } ); }
    // 刻度 7 格（0~3000 的步长取整成 500，网格细一倍），图也给高一些。
    //
    // ★ 三个档位阈值是 348 / 127 / 54，在 0~3000 的纵轴上只占底部 12%，
    //   必须靠绝对高度把它们拉开 —— 绘图区高度 = height - 88（上下边距）。
    //   300px 高时三条线离底边只有 22 / 8 / 3.5 像素，几乎重叠；
    //   560px 高（绘图区 472px）则拉开到 55 / 20 / 8.5 像素，能分得清。
    const std::vector<ChartSeries> series = { { "", d.recoil_curve, COLOR_FIRST } };
    // 单线图，series 名字留空 → 悬停时用纵轴名当标签
    ChartOpts o;
    o.y_label = zh::g::AXIS_RECOIL;
    o.y_ticks = 7;
    o.height  = 560.0f;
    o.marks   = &marks;
    draw_line_chart( "##recoil_curve", series, o );
}

// 通用折线图：X 轴固定是「瞄准回合」，Y 轴由调用方给名字和格式。
// data[i] = 第 i 回合的值。marks 是可选的参考横线。
//
// 自己用 ImDrawList 画，没引 ImPlot —— 见文件头「图表是手绘的」那段。
//
// 坐标轴、网格、折线、填充、刻度取整、参考线、悬停提示都在这一个函数里，
// 两个图（瞄准收益曲线、瞄准时间线的误差曲线）共用。
void draw_line_chart( const char *id, const std::vector<ChartSeries> &series,
                      const ChartOpts &opts )
{
    if( series.empty() ) {
        return;
    }
    const int N = (int)series[0].data.size();
    if( N < 2 ) {
        return;
    }

    const char *y_label = opts.y_label;
    const char *y_fmt   = opts.y_fmt;
    const std::vector<ChartMark> &marks =
        opts.marks ? *opts.marks : std::vector<ChartMark>{};

    // 每张图各自的缩放/平移/高度，键是图的 id
    ChartView &view = g_chart_views[id];

    const float  W      = std::max( 360.0f, ImGui::GetContentRegionAvail().x - 8.0f );
    const float  H      = ( view.height > 0.0f ) ? view.height : opts.height;
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton( id, ImVec2( W, H ) );
    const bool   hovered = ImGui::IsItemHovered();
    const bool   active  = ImGui::IsItemActive();
    const ImVec2 mouse   = ImGui::GetIO().MousePos;

    // ---- 拖动平移、滚轮缩放、双击复位 --------------------------------------
    // 缩放以光标下的那个点为锚（不然放大后视野会乱飘）。
    // 平移用 MouseDelta 换算成「归一化下标」的位移量。
    {
        const float plot_w = std::max( 1.0f, W - 56.0f - 18.0f );   // 与下面的 L/R 一致
        if( active && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) ) {
            view.cx -= ImGui::GetIO().MouseDelta.x / plot_w / view.zoom;
        }
        if( hovered && ImGui::GetIO().MouseWheel != 0.0f ) {
            // 光标在视野里的归一化位置（0~1）
            const float fx = std::clamp( ( mouse.x - origin.x - 56.0f ) / plot_w,
                                         0.0f, 1.0f );
            const float half_old = 0.5f / view.zoom;
            const float ux = ( view.cx - half_old ) + fx * ( 2.0f * half_old );
            view.zoom = std::clamp( view.zoom * std::pow( 1.15f, ImGui::GetIO().MouseWheel ),
                                    1.0f, 20.0f );
            const float half_new = 0.5f / view.zoom;
            view.cx = ux - fx * ( 2.0f * half_new ) + half_new;
        }
        if( hovered && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
            view.zoom = 1.0f;
            view.cx   = 0.5f;
        }
        if( active && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeAll );
        }
        // 视野中心不能越界（整段数据就是 [0,1]）
        const float half = 0.5f / view.zoom;
        view.cx = std::clamp( view.cx, half, 1.0f - half );
    }

    ImDrawList *dl = ImGui::GetWindowDrawList();

    // 绘图区。四周的边距留给坐标轴：左边放 Y 刻度数字，下面放 X 刻度数字，
    // 上面一行放 Y 轴名。B 要够大 —— 刻度数字和轴名是上下两行，留窄了会叠在一起
    const float L = 56.0f, R = 18.0f, T = 36.0f, B = 52.0f;
    const ImVec2 a( origin.x + L,     origin.y + T );
    const ImVec2 b( origin.x + W - R, origin.y + H - B );

    // Y 轴范围要把参考线也框进来，否则阈值线会画到图外面
    double maxY = 1.0;
    for( const ChartSeries &s : series ) {
        for( double v : s.data ) {
            maxY = std::max( maxY, v );
        }
    }
    for( const ChartMark &m : marks ) {
        maxY = std::max( maxY, m.y );
    }
    const double step = nice_step( maxY, opts.y_ticks );
    const double top  = std::max( step, std::ceil( maxY / step ) * step );

    // 可见的横轴范围（归一化下标 0~1）。zoom=1 时就是 [0,1] 全部。
    const float view_half = 0.5f / view.zoom;
    const float x0 = view.cx - view_half;
    const float x1 = view.cx + view_half;

    auto px = [&]( int t ) {
        const float u = (float)t / (float)std::max( 1, N - 1 );   // 归一化下标
        return a.x + ( b.x - a.x ) * ( u - x0 ) / ( x1 - x0 );
    };
    auto py = [&]( double v ) {
        return b.y - ( b.y - a.y ) * (float)( v / top );
    };
    // 「归一化下标 → 采样点序号」，悬停提示用
    auto u_to_index = [&]( float u ) {
        return std::clamp( (int)std::lround( u * ( N - 1 ) ), 0, N - 1 );
    };
    // 可见的采样点范围（放大后只画这一截，省得白算）
    const int vis_lo = u_to_index( x0 );
    const int vis_hi = u_to_index( x1 );

    const ImU32 col_bg   = ImGui::GetColorU32( ImGuiCol_FrameBg );
    const ImU32 col_grid = ImGui::GetColorU32( ImGuiCol_Border, 0.7f );
    const ImU32 col_axis = ImGui::GetColorU32( ImGuiCol_Text, 0.45f );
    const ImU32 col_text = ImGui::GetColorU32( ImGuiCol_Text, 0.60f );
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

    // 纵向网格 + X 轴刻度（点多了就隔几个标一个，别挤成一团）。
    // ★ 步长随缩放走：放大之后看到的点少了，再按原来的间隔标就太稀，
    //   所以除以 zoom —— 视觉上刻度密度始终差不多。
    const bool custom_x = ( (int)opts.x_labels.size() == N );
    const int base_step  = std::max( 1, ( N - 1 ) / 10 );                  // 网格线
    const int base_lstep = opts.x_tick_step > 0 ? opts.x_tick_step : base_step;
    const int xstep = std::max( 1, (int)std::lround( base_step  / view.zoom ) );
    const int lstep = std::max( 1, (int)std::lround( base_lstep / view.zoom ) );
    for( int t = vis_lo; t <= vis_hi; t += xstep ) {
        const float x = px( t );
        dl->AddLine( ImVec2( x, a.y ), ImVec2( x, b.y ), col_grid );
    }
    for( int t = vis_lo; t <= vis_hi; t += lstep ) {
        const std::string lab = custom_x ? opts.x_labels[t] : fmt_str( "%d", t );
        const ImVec2 ts = ImGui::CalcTextSize( lab.c_str() );
        dl->AddText( ImVec2( px( t ) - ts.x * 0.5f, b.y + 6.0f ), col_text, lab.c_str() );
    }

    // 坐标轴
    dl->AddLine( ImVec2( a.x, b.y ), ImVec2( b.x, b.y ), col_axis, 1.5f );
    dl->AddLine( ImVec2( a.x, a.y ), ImVec2( a.x, b.y ), col_axis, 1.5f );

    // 折线。多条线时不画填充 —— 半透明色块互相叠加会糊成一片，
    // 反而看不清谁是谁。单条线时垫一层，看着有分量。
    //
    // ★ 放大之后曲线会伸到绘图区外面，必须裁剪 —— 不然线会画到坐标轴上、
    //   甚至糊到旁边的图例里。只裁数据，网格和坐标轴在边上，不能裁。
    const bool fill = ( series.size() == 1 );

    dl->PushClipRect( a, b, true );
    for( const ChartSeries &s : series ) {
        std::vector<ImVec2> pts;
        pts.reserve( N );
        for( int t = 0; t < N; t++ ) {
            pts.emplace_back( px( t ), py( s.data[t] ) );
        }

        if( fill ) {
            const ImU32 col_fill = ( s.color & 0x00FFFFFF ) | ( 40u << 24 );
            for( int t = 0; t + 1 < N; t++ ) {
                dl->AddQuadFilled( pts[t], pts[t + 1],
                                   ImVec2( pts[t + 1].x, b.y ), ImVec2( pts[t].x, b.y ),
                                   col_fill );
            }
        }

        dl->AddPolyline( pts.data(), N, s.color, ImDrawFlags_None, 2.0f );
        for( const ImVec2 &p : pts ) {
            dl->AddCircleFilled( p, 3.0f, s.color );
        }
    }
    dl->PopClipRect();

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

    // 右上角图例：先列曲线，再列参考横线。
    //
    // 参考线的标签不画在线上 —— 阈值通常远小于纵轴上限（348 / 127 / 54 对
    // 3000），三条线全挤在最下面一截，各自带标签会叠成一团。
    // 右上角一定是空的：本项目的曲线都是从左上降到右下。
    //
    // ★ 参考线数值用 (int) 截断而不是 %.0f —— 阈值是小数（348.6），
    //   四舍五入会显示成 349，和别处（用截断）差一个数，看着像 bug。
    {
        struct LegendItem {
            std::string text;
            ImU32       color;
        };
        std::vector<LegendItem> items;
        for( const ChartSeries &s : series ) {
            if( !s.name.empty() ) {
                items.push_back( { s.name, s.color } );
            }
        }
        for( const ChartMark &m : marks ) {
            items.push_back( { fmt_str( zh::g::LEGEND_FMT, m.label, (int)m.y ), col_mark } );
        }

        if( !items.empty() ) {
            float lw = 0.0f;
            for( const LegendItem &it : items ) {
                lw = std::max( lw, ImGui::CalcTextSize( it.text.c_str() ).x );
            }

            const float lineH = ImGui::GetTextLineHeight();
            const float pad   = 6.0f, swatch = 16.0f;
            const ImVec2 p0( b.x - lw - swatch - pad * 3.0f, a.y + pad );
            const ImVec2 p1( b.x - pad, p0.y + lineH * (float)items.size() + pad * 2.0f );

            dl->AddRectFilled( p0, p1, IM_COL32( 16, 16, 20, 215 ), 3.0f );
            dl->AddRect( p0, p1, col_grid, 3.0f );

            float ty = p0.y + pad;
            for( const LegendItem &it : items ) {
                const float cy = ty + lineH * 0.5f;
                dl->AddLine( ImVec2( p0.x + pad, cy ), ImVec2( p0.x + pad + swatch, cy ),
                             it.color, 2.0f );
                dl->AddText( ImVec2( p0.x + pad + swatch + pad, ty ), it.color,
                             it.text.c_str() );
                ty += lineH;
            }
        }
    }

    // 悬停：竖线 + 该回合的数值。
    // ★ 缩小/放大之后，屏幕上的一段对应的是视野 [x0,x1] 那一段，不能直接按
    //   「鼠标在绘图区的比例 × (N−1)」算 —— 那样放大后会指到错误的点。
    if( hovered && mouse.x >= a.x - 6.0f && mouse.x <= b.x + 6.0f ) {
        const float fx = std::clamp( ( mouse.x - a.x ) / ( b.x - a.x ), 0.0f, 1.0f );
        const int   t  = u_to_index( x0 + fx * ( x1 - x0 ) );
        const ImVec2 p( px( t ), a.y );
        dl->AddLine( ImVec2( p.x, a.y ), ImVec2( p.x, b.y ),
                     ImGui::GetColorU32( ImGuiCol_Text, 0.3f ) );

        // 每条线各标一个点、各报一个数 —— 多条线时只报一条没法比。
        //
        // ★ 这里用的是**字面量**格式串，不是从 opts 传进来的变量。
        //   之前传变量（"%d 回合\n%.0f 格"），改成「每条线一行」后忘了同步改，
        //   结果 %d 把字符串指针当整数打印，提示里出现 -459279152 这种鬼数字。
        //   字面量的话编译器能查，而且少一处要同步维护的东西。
        ImGui::BeginTooltip();
        // 第一行是横轴那个点的值。横轴是回合时显示「N 回合」；
        // 用了自定义刻度（比如误差那张图）就显示刻度本身 + 轴名。
        if( custom_x ) {
            ImGui::TextUnformatted(
                fmt_str( "%s %s%s", opts.x_label, opts.x_labels[t].c_str(),
                         opts.x_unit ).c_str() );
        } else {
            ImGui::Text( zh::g::TIP_TURN, t );
        }
        for( const ChartSeries &s : series ) {
            dl->AddCircleFilled( ImVec2( px( t ), py( s.data[t] ) ), 5.0f, s.color );
            const char *label = s.name.empty() ? y_label : s.name.c_str();
            if( s.color != 0 ) {
                ImGui::PushStyleColor( ImGuiCol_Text, s.color );
                ImGui::Text( "%s  %.0f%s", label, s.data[t], opts.y_unit );
                ImGui::PopStyleColor();
            } else {
                ImGui::Text( "%s  %.0f%s", label, s.data[t], opts.y_unit );
            }
        }
        ImGui::EndTooltip();
    }

    // 轴名。这两个是字面量，直接画 —— 别丢进 printf，里面那个 % 会被当格式符。
    // Y 轴名放绘图区左上角（竖排太麻烦），X 轴名居中放在刻度下面
    dl->AddText( ImVec2( a.x, origin.y + 6.0f ), col_text, y_label );
    {
        const ImVec2 ts = ImGui::CalcTextSize( opts.x_label );
        dl->AddText( ImVec2( ( a.x + b.x ) * 0.5f - ts.x * 0.5f, b.y + 28.0f ),
                     col_text, opts.x_label );
    }

    // ---- 底边的拖拽条：往下拖把图拉高 --------------------------------------
    // 和左右分栏那条竖线一个套路：透明按钮接鼠标、按 MouseDelta 改值、自己画线。
    {
        // ★ ID 必须带上图自己的 id —— 三张图的拖拽条在同一个窗口里，
        //   全叫 "##hsplit" 会撞 ID（ImGui 会直接弹红框报 Programmer error）。
        const std::string grip_id = std::string( id ) + "_grip";
        ImGui::SetCursorScreenPos( ImVec2( origin.x, origin.y + H ) );
        ImGui::InvisibleButton( grip_id.c_str(), ImVec2( W, 7.0f ) );
        const bool gact = ImGui::IsItemActive();
        const bool ghov = ImGui::IsItemHovered();
        // ★ clamp 必须只在拖动时做。view.height 的初值 0 是「用 opts.height」
        //   的哨兵，无条件 clamp 会在第一帧就把它夹成下限（140），
        //   于是所有图都变成最矮的——踩过这个坑。
        if( gact ) {
            view.height = std::clamp( H + ImGui::GetIO().MouseDelta.y,
                                      140.0f, 1400.0f );
        }
        if( gact || ghov ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeNS );
        }
        const ImVec2 q0 = ImGui::GetItemRectMin();
        const ImVec2 q1 = ImGui::GetItemRectMax();
        const float  gy = ( q0.y + q1.y ) * 0.5f;
        const float  gx0 = origin.x + ( W - 60.0f ) * 0.5f;      // 中间画一小段，别画满
        dl->AddLine( ImVec2( gx0, gy ), ImVec2( gx0 + 60.0f, gy ),
                     ImGui::GetColorU32( gact  ? ImGuiCol_SeparatorActive
                                       : ghov  ? ImGuiCol_SeparatorHovered
                                               : ImGuiCol_Separator ),
                     gact ? 3.0f : 1.0f );
    }

    // 图的交互方式没有视觉提示，不写一句没人会知道能拖/能缩放
    ImGui::TextDisabled( "%s", zh::g::CHART_OPS );
}

// 卡壳警告：选中的弹药「推不动这把枪的循环」时弹一条醒目的。
//
// ★ 游戏里卡壳**不会**中断当前这一轮连发。min_cycle_recoil 那段代码明确
//   不返回 false（注释就写着 "Don't return false in this case"），而且
//   fault_gun_chamber_spent 在 0.I 全源码里只出现 3 次 —— 声明、判定、设置，
//   **没有任何地方检查它来阻止开火**。真正的代价在后面：之后每次想开火，
//   瞄准活动入口（activity_actor.cpp:352）会先扣约一回合的行动点、把 recoil
//   重置回 MAX_RECOIL（瞄准进度清零），再有 1/max(7, 15-4×枪械技能) 的概率
//   当场修好，修不好这次开火就作废。
//
//   所以这条警告不改任何算出来的数，只是告诉玩家「这么配弹药会很难受」。
void draw_jam_warning( const Gun &g, const Ammo *ammo )
{
    if( ammo == nullptr || !g.can_jam || g.min_cycle_recoil <= 0.0 ) {
        return;                       // 这枪压根不会卡（转轮/手动枪机/发射器）
    }
    if( ammo->recoil >= g.min_cycle_recoil ) {
        return;                       // 后坐力够，推得动
    }

    ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 140, 110, 255 ) );
    ImGui::PushTextWrapPos( 0.0f );
    ImGui::TextUnformatted( fmt_str( zh::g::JAM_TITLE_FMT, (int)ammo->recoil,
                                     (int)g.min_cycle_recoil ).c_str() );
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    note( "%s", zh::g::JAM_DETAIL );
}

// 逐发明细：连发模式下一轮里每一发能打多远。
//
// 只负责显示 —— 所有数值（含每发的概率）都在 compute_detail 里算好缓存了。
// 概率不能放这儿算：绘图每帧都跑，而每发都要采样。
void draw_burst_table( const DetailCache &d )
{
    for( const DetailCache::SustainedCurve &sc : d.sustained ) {
        if( sc.qty <= 1 || sc.burst_shot_recoil.empty() ) {
            continue;
        }

        ImGui::Spacing();
        ImGui::TextUnformatted(
            fmt_str( zh::g::BURST_HDR_FMT, sc.qty, d.burst_interval,
                     sc.burst_start_recoil ).c_str() );

        ImGui::PushID( sc.qty );
        if( ImGui::BeginTable( "burst", 4,
                               TABLE_FLAGS )) {
            ImGui::TableSetupColumn( zh::g::COL_SHOT_NO, ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( zh::g::COL_RECOIL, ImGuiTableColumnFlags_WidthFixed, 100 );
            ImGui::TableSetupColumn( zh::g::COL_50RANGE, ImGuiTableColumnFlags_WidthFixed, 120 );
            ImGui::TableSetupColumn( zh::g::COL_GOODPLUS, ImGuiTableColumnFlags_WidthFixed, 140 );
            ImGui::TableHeadersRow();

            for( size_t i = 0; i < sc.burst_shot_recoil.size(); i++ ) {
                const double rec = sc.burst_shot_recoil[i];
                const int rng_tiles =
                    range_with_even_chance_of_good_hit( d.fixed_disp + rec );

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text( zh::g::SHOT_NO_FMT, (int)i + 1 );
                ImGui::TableNextColumn(); ImGui::Text( "%d", (int)rec );
                ImGui::TableNextColumn();
                if( rng_tiles >= 59 ) ImGui::TextUnformatted( zh::g::OVER_TABLE );
                else                  ImGui::Text( zh::g::TILE_FMT, rng_tiles );
                ImGui::TableNextColumn();
                if( i < sc.burst_goodplus.size() ) {
                    ImGui::Text( "%.1f%%", 100.0 * sc.burst_goodplus[i] );
                }
            }
            ImGui::EndTable();
        }
        ImGui::PopID();

        if( sc.burst_shown < sc.qty ) {
            ImGui::TextDisabled( zh::g::BURST_TRUNC, sc.qty - sc.burst_shown );
        }
        // 连发中途的误差可以超过 3000 —— 上限只在打完一轮时才生效
        if( sc.burst_over_cap ) {
            note( "%s", zh::g::BURST_OVER_CAP );
        }
    }
}

// 有没有连发模式（一轮多发）—— 没有的话就不用显示逐发明细那一套
bool has_multi_shot( const DetailCache &d )
{
    for( const DetailCache::SustainedCurve &sc : d.sustained ) {
        if( sc.qty > 1 && !sc.burst_shot_recoil.empty() ) {
            return true;
        }
    }
    return false;
}

// 瞄准收益曲线：横轴 = 瞄准回合，纵轴 = 50%好击距离
void draw_range_curve( const DetailCache &d )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_CURVE, ImGuiTreeNodeFlags_DefaultOpen ) ) {
        return;
    }
    note( "%s", zh::g::CURVE_HINT );
    note( "%s", zh::g::SUSTAINED_HINT );

    // 三条线（或更多）画在同一张图上：
    //   首次开火 —— 冷启动，就是左边那条 d.curve
    //   每个射击模式一条「持续」，即反复「瞄 N 回合 → 开火」稳定下来的水平
    const ImU32 palette[] = { COLOR_SUST_1, COLOR_SUST_2, COLOR_SUST_3 };
    std::vector<ChartSeries> series;
    series.push_back( { zh::g::LINE_FIRST, d.curve, COLOR_FIRST } );
    for( size_t i = 0; i < d.sustained.size(); i++ ) {
        series.push_back( { d.sustained[i].label, d.sustained[i].range,
                            palette[i % 3] } );
    }

    ChartOpts o;
    o.y_label = zh::g::AXIS_RANGE;
    o.y_unit  = " 格";
    o.y_ticks = 5;
    o.height  = 340.0f;
    draw_line_chart( "##range_curve", series, o );

    // 逐发明细的间隔控件。放在这儿是为了让它和图的横轴对上 ——
    // 图上看中哪个位置，把间隔调过去就能看那一轮的逐发明细。
    if( has_multi_shot( d ) ) {
        ImGui::Spacing();
        ImGui::TextUnformatted( zh::g::BURST_INTERVAL );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 90 );
        // 用 InputInt（带 +/- 按钮）而不是 Slider：拖动条每帧都在变，
        // 每次变都要重算整套（含 12 发 × 2 万次采样），会卡。
        ImGui::InputInt( "##burst_interval", &g_burst_interval, 1, 1 );
        ImGui::SameLine();
        ImGui::TextDisabled( "%s", zh::g::BURST_TURNS );
        g_burst_interval = std::max( 0, std::min( CURVE_MAX_TURNS, g_burst_interval ) );

        draw_burst_table( d );
    }
}

// 瞄准档位实例
void draw_instance( const DetailCache &d )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_INSTANCE ) ) {
        return;
    }
    note( "%s", zh::g::INSTANCE_HINT );

    if( ImGui::BeginTable( "instance", 5,
                           TABLE_FLAGS )) {
        ImGui::TableSetupColumn( zh::g::COL_AIMLEVEL, ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( zh::g::COL_RECOIL, ImGuiTableColumnFlags_WidthFixed, 100 );
        ImGui::TableSetupColumn( zh::g::COL_FIXDISP, ImGuiTableColumnFlags_WidthFixed, 100 );
        ImGui::TableSetupColumn( zh::g::COL_TOTDISP, ImGuiTableColumnFlags_WidthFixed, 100 );
        ImGui::TableSetupColumn( zh::g::COL_50RANGE, ImGuiTableColumnFlags_WidthFixed, 140 );
        ImGui::TableHeadersRow();

        // 完全没瞄 = 最大后坐；另外三档直接用 simulate_aim 算好的阈值
        // （那里已经照游戏做了 int 截断和去重，别再在这儿重算一遍公式）。
        // 不存在的档位（阈值被去重掉的）不列。
        const struct { const char *name; double recoil; } rows[] = {
            { zh::AIM_LEVEL_0, MAX_RECOIL },
            { zh::AIM_LEVEL_1, d.aim.regular_th },
            { zh::AIM_LEVEL_2, d.aim.careful_th },
            { zh::AIM_LEVEL_3, d.aim.precise_th },
        };
        for( int i = 0; i < 4; i++ ) {
            if( i == 2 && !d.aim.has_careful ) continue;
            if( i == 3 && !d.aim.has_precise ) continue;
            const double total = d.fixed_disp + rows[i].recoil;
            const int rng = range_with_even_chance_of_good_hit( total );
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted( rows[i].name );
            // ★ 一律用 (int) 截断，不用 %.0f。固定散布这类值是小数（299.5），
            //   四舍五入会显示成 300，和同屏别处（用截断）差一个数，看着像 bug。
            ImGui::TableNextColumn(); ImGui::Text( "%d", (int)rows[i].recoil );
            ImGui::TableNextColumn(); ImGui::Text( "%d", (int)d.fixed_disp );
            ImGui::TableNextColumn(); ImGui::Text( "%d", (int)total );
            ImGui::TableNextColumn();
            if( rng >= 59 ) ImGui::TextUnformatted( zh::g::OVER_TABLE );
            else            ImGui::Text( zh::g::TILE_FMT, rng );
        }
        ImGui::EndTable();
    }
}

// 瞄准误差 → 各命中档位概率。横轴是瞄准误差本身（不是回合），
// 六条线是六个档位。距离用左下角的控件调 —— 命中档位还取决于距离。
void draw_tier_curve( const DetailCache &d )
{
    if( !ImGui::CollapsingHeader( zh::g::SEC_TIERCURVE, ImGuiTreeNodeFlags_DefaultOpen ) ) {
        return;
    }
    note( "%s", zh::g::TIERCURVE_HINT );

    ImGui::TextUnformatted( zh::g::TIER_DIST );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 90 );
    ImGui::InputInt( "##tier_dist", &g_tier_dist, 1, 5 );
    ImGui::SameLine();
    ImGui::TextDisabled( "%s", zh::g::TIER_DIST_UNIT );
    g_tier_dist = std::max( 1, std::min( 59, g_tier_dist ) );

    // 横轴刻度：每个点都存（悬停提示要用准确值），轴上每 3 个画一个
    std::vector<std::string> labels;
    labels.reserve( d.tier_err.size() );
    for( double e : d.tier_err ) {
        labels.push_back( fmt_str( "%.0f", e ) );
    }

    // 从 1 开始 —— 0 是「爆头」，那个给不出来（见 compute_detail 里的说明），
    // 恒为 0 的一条线画出来只会误导。
    std::vector<ChartSeries> series;
    for( int t = 1; t < 6; t++ ) {
        series.push_back( { zh::hit_tier_short( t ), {}, TIER_COLORS[t] } );
        series.back().data.assign( d.tier_pct[t].begin(), d.tier_pct[t].end() );
        // 图上的纵轴用百分比，比 0~1 好读
        for( double &v : series.back().data ) { v *= 100.0; }
    }

    // 纵轴刻度直接带百分号（"%.0f%%" 走 printf，所以百分号要写两个）；
    // 悬停提示用的是字面量格式串，单位从那边的 y_unit 补。
    ChartOpts o;
    o.y_label  = zh::g::AXIS_TIER_PCT;
    o.x_label  = zh::g::AXIS_RECOIL;
    o.y_unit   = "%";
    o.y_fmt    = "%.0f%%";
    o.y_ticks  = 5;
    o.height   = 340.0f;
    o.x_labels = labels;
    o.x_tick_step = 3;                 // 22 个点全标会糊成一片
    draw_line_chart( "##tier_curve", series, o );
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
    draw_jam_warning( g, ammo );      // 放在最上面：它影响下面所有数字的可信度
    if( ammo == nullptr ) {
        // 模块化枪械没装机匣时没有口径，但配件还是要能装 —— 装备区不能藏
        ImGui::Spacing();
        draw_mods( g );
        ImGui::EndChild();
        return;
    }

    if( !key_matches_detail( g_detail ) ) {
        compute_detail( g, ammo );
    }

    ImGui::Spacing();
    draw_mods( g );
    draw_aim_params( g, g_detail );
    draw_game_values( g, ammo );
    draw_aim_levels( g_detail );
    draw_aim_timeline( g_detail );
    draw_range_curve( g_detail );
    draw_tier_curve( g_detail );
    draw_instance( g_detail );

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

    // 分栏那一行的总宽，要在画列表**之前**取 —— 之后取就只剩「剩余宽度」了，
    // 见 draw_splitter 的说明
    const float split_full_w = ImGui::GetContentRegionAvail().x;
    draw_gun_list();
    draw_splitter( split_full_w );
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
