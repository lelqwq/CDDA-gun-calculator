// =============================================================================
//  main_gui.cpp  —  图形界面版（SDL3 + ImGui）
// -----------------------------------------------------------------------------
//  与命令行版（src/gunlab.cpp）共用 gunlab_core，所以两边算出来的数字一致。
//  界面只负责"把数字摆出来"，不做任何计算。
//
//  渲染用的是 SDL3 自带的 SDL_Renderer（不是 OpenGL）—— 省掉 GL 函数加载，
//  依赖更少，对这种数据工具完全够用。
//
//  ★ 中文字体必须手动加载，否则全是方块（ImGui 内置字体不含 CJK）。
// =============================================================================

#include <algorithm>
#include <cstdio>
#include <filesystem>
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

char        g_search[128] = "";      // 搜索关键词
std::vector<int> g_hits;             // 匹配到的枪械下标
int         g_selected = -1;         // 当前选中的枪械下标

// 按关键词重新筛选（空关键词 = 全部）
void refresh_hits()
{
    g_hits = search_guns( g_search );
}

// 顶部：搜索框 + 计数
void draw_toolbar()
{
    ImGui::SetNextItemWidth( 320 );
    if( ImGui::InputTextWithHint( "##search", "搜索：中文名 / 英文名 / id / 别名",
                                  g_search, sizeof( g_search ) ) ) {
        refresh_hits();
        g_selected = -1;
    }
    ImGui::SameLine();
    if( ImGui::Button( "清空" ) ) {
        g_search[0] = '\0';
        refresh_hits();
        g_selected = -1;
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "%d / %d 把", (int)g_hits.size(), (int)g_guns.size() );
}

// 左侧：枪械列表
void draw_gun_list()
{
    ImGui::BeginChild( "list", ImVec2( 380, 0 ), ImGuiChildFlags_Borders );
    if( ImGui::BeginTable( "guns", 3,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                           ImGuiTableFlags_ScrollY ) ) {
        ImGui::TableSetupScrollFreeze( 0, 1 );
        ImGui::TableSetupColumn( "名称", ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableSetupColumn( "技能", ImGuiTableColumnFlags_WidthFixed, 56 );
        ImGui::TableSetupColumn( "重量", ImGuiTableColumnFlags_WidthFixed, 72 );
        ImGui::TableHeadersRow();

        for( int idx : g_hits ) {
            const Gun &g = g_guns[idx];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID( idx );
            if( ImGui::Selectable( g.name.c_str(), g_selected == idx,
                                   ImGuiSelectableFlags_SpanAllColumns ) ) {
                g_selected = idx;
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

// 右侧：详情（第 3 步会填充完整内容）
void draw_detail()
{
    ImGui::BeginChild( "detail", ImVec2( 0, 0 ), ImGuiChildFlags_Borders );
    if( g_selected < 0 ) {
        ImGui::TextDisabled( "从左边选一把枪" );
        ImGui::EndChild();
        return;
    }

    const Gun &g = g_guns[g_selected];
    const Ammo *ammo = pick_default_ammo( g );

    ImGui::TextUnformatted( g.name.c_str() );
    ImGui::SameLine();
    ImGui::TextDisabled( "(%s)", g.id.c_str() );
    ImGui::Separator();

    ImGui::Text( "技能：%s", zh::skill( g.skill ).c_str() );
    ImGui::Text( "重量：%.0f g    体积：%.0f ml",
                 effective_weight( g ), effective_volume( g ) );
    if( ammo ) {
        ImGui::Text( "弹药：%s", ammo->name.c_str() );
    } else {
        ImGui::TextDisabled( "弹药：（无适配弹药，可能需要先装上机匣）" );
    }

    ImGui::Spacing();
    ImGui::SeparatorText( "游戏内显示值" );

    // 与游戏物品界面对齐的四项（0.I 格式：分项相加，原始值）
    const int d_gun  = (int)game_dispersion_gun( g );
    const int d_ammo = (int)game_dispersion_ammo( g, ammo );

    ImGui::Text( "散布（枪身+弹药）" );
    ImGui::SameLine( 220 );
    ImGui::Text( "%d+%d = %d", d_gun, d_ammo, d_gun + d_ammo );

    ImGui::Text( "实际后坐" );
    ImGui::SameLine( 220 );
    ImGui::Text( "%.0f", game_recoil( g, Character{}, ammo ) );

    ImGui::Text( "理论最小后坐力" );
    ImGui::SameLine( 220 );
    ImGui::Text( "%.0f（所需力量 %d）", game_min_recoil( g, Character{}, ammo ),
                 (int)( gun_base_weight( g ) / 333.0 ) );

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

int main( int, char ** )
{
    init_database();
    refresh_hits();

    if( !SDL_Init( SDL_INIT_VIDEO ) ) {
        std::printf( "SDL_Init 失败：%s\n", SDL_GetError() );
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow( "gunlab — Cataclysm 枪械计算器",
                                        1400, 900,
                                        SDL_WINDOW_RESIZABLE |
                                        SDL_WINDOW_HIGH_PIXEL_DENSITY );
    if( win == nullptr ) {
        std::printf( "创建窗口失败：%s\n", SDL_GetError() );
        SDL_Quit();
        return 1;
    }

    // 显式显示并提到前台（虽然 SDL3 默认就会显示，但某些环境下不会自动置顶）
    SDL_ShowWindow( win );
    SDL_RaiseWindow( win );

    SDL_Renderer *ren = SDL_CreateRenderer( win, nullptr );
    if( ren == nullptr ) {
        std::printf( "创建渲染器失败：%s\n", SDL_GetError() );
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
