# =============================================================================
#  CMakeLists.txt  —  gunlab
# -----------------------------------------------------------------------------
#  项目只剩图形界面版（SDL3 + ImGui），用 scripts\build_gui.bat 一键构建。
#
#  ★ 必须用 MinGW，不能用 MSVC：机器上唯一的 SDL3 是 msys64 的 MinGW 版，
#    MSVC 链不了它。命令行版删掉之后，MSVC 就彻底用不上了。
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(gunlab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 编译选项 ---------------------------------------------------------------
if(MSVC)
    # /utf-8 是必须的：源码是 UTF-8，但 MSVC 默认按系统代码页（简体中文是 936）
    # 解读，会把中文字符串字面量解坏，导致一堆假语法错误。
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra)

    # MinGW 下静态链接运行库，否则 exe 需要 libgcc_s_seh-1.dll / libstdc++-6.dll，
    # 直接双击运行时这些 DLL 不在 PATH 上，会报「找不到 xxx.dll」。
    if(MINGW)
        add_link_options(-static -static-libgcc -static-libstdc++)
    endif()
endif()

# ---- 核心库 -----------------------------------------------------------------
# 数据 + 公式，不含任何输入输出。命令行版与图形版共用这一层。
add_library(gunlab_core STATIC
    src/gun_data.cpp
    src/gunlab_math.cpp
    # 由 scripts/gen_gun_data.py 从游戏数据生成，请勿手工编辑
    src/generated/gen_guns.cpp
    src/generated/gen_ammo.cpp
    src/generated/gen_gunmods.cpp
)
target_include_directories(gunlab_core PUBLIC src)

# ---- 图形界面版（需要 SDL3）-------------------------------------------------
# 渲染用 SDL3 自带的 SDL_Renderer，不依赖 OpenGL 加载器。
find_package(SDL3 QUIET)

if(SDL3_FOUND)
    add_library(imgui STATIC
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/imgui_stdlib.cpp
        third_party/imgui/imgui_impl_sdl3.cpp
        third_party/imgui/imgui_impl_sdlrenderer3.cpp
    )
    target_include_directories(imgui PUBLIC third_party/imgui)
    target_link_libraries(imgui PUBLIC SDL3::SDL3)

    # 第三方代码，关掉警告
    if(MSVC)
        target_compile_options(imgui PRIVATE /w)
    else()
        target_compile_options(imgui PRIVATE -w)
    endif()

    add_executable(gunlab_gui src/gui/main_gui.cpp)
    target_link_libraries(gunlab_gui PRIVATE gunlab_core imgui)
    # 自己接管 main（否则 MinGW 下 SDL3 会重定向到 SDL_main）
    target_compile_definitions(gunlab_gui PRIVATE SDL_MAIN_HANDLED)

    set_target_properties(gunlab_gui PROPERTIES
        # ★ 必须设成 GUI 子系统，否则 Windows 会给它挂一个控制台窗口 ——
        #   双击时先弹一个黑框，很难看。MinGW 下等价于 -mwindows。
        #   代价是没有了 stdout：启动诊断只在重定向到文件时才收得到，
        #   所以启动失败的提示改用消息框（见 main_gui.cpp 的 fatal()）。
        WIN32_EXECUTABLE TRUE
        RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    )
else()
    message(FATAL_ERROR
        "未找到 SDL3。\n"
        "  图形界面版需要 SDL3，msys64 里装一个：\n"
        "      pacman -S mingw-w64-x86_64-SDL3\n"
        "  然后用 scripts\\build_gui.bat 构建（它会带上 msys64 的前缀路径）。\n"
        "  注意别用 MSVC 生成器 —— 它链不了 MinGW 编译的 SDL3。")
endif()
