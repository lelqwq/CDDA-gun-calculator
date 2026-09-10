# =============================================================================
#  CMakeLists.txt  —  gunlab
# -----------------------------------------------------------------------------
#  用法：
#     cmake -B build -S .
#     cmake --build build
#     然后运行 build\gunlab.exe（Windows）或 build/gunlab（Linux/macOS）
#
#  也可以直接用 scripts\ 下的 .bat 脚本编译（不依赖 CMake）。
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

# ---- 命令行版 ---------------------------------------------------------------
add_executable(gunlab src/gunlab.cpp)
target_link_libraries(gunlab PRIVATE gunlab_core)

# ---- 输出路径 ---------------------------------------------------------------
# 让 exe 直接落在 build/ 下，而不是 build/Debug/ 之类的子目录（Visual Studio
# 这类多配置生成器会有子目录，Ninja/Make 不会）。统一一下，方便脚本调用。
set_target_properties(gunlab PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
)
