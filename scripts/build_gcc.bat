@echo off
REM ============================================================================
REM  scripts\build_gcc.bat - Build gunlab with the MSYS2 MinGW-w64 g++
REM  (ASCII only on purpose: cmd.exe uses the OEM codepage, not UTF-8)
REM ============================================================================
setlocal
REM switch to the project root (%~dp0 ends with a backslash)
cd /d "%~dp0.."

set "MINGW=C:\msys64\mingw64\bin"
if not exist "%MINGW%\g++.exe" (
    echo [ERROR] g++.exe not found at %MINGW%
    echo         Edit the MINGW path in this script.
    pause
    exit /b 1
)

REM The compiler needs its own DLLs on PATH, otherwise it fails silently
REM (exit code 1, no error message at all).
set "PATH=%MINGW%;%PATH%"

echo [1/2] Creating build directory...
if not exist build mkdir build

echo [2/2] Compiling src\gunlab.cpp + src\gun_data.cpp ...
REM -static makes the .exe self-contained: without it the binary needs
REM libgcc_s_seh-1.dll / libstdc++-6.dll at run time, which are NOT on PATH
REM when you double-click the exe from Explorer.
g++ -std=c++17 -O2 -Wall -Wextra -Isrc -static -static-libgcc -static-libstdc++ ^
    -o build\gunlab_gcc.exe ^
    src\gunlab.cpp src\gun_data.cpp src\gunlab_math.cpp ^
    src\generated\gen_guns.cpp src\generated\gen_ammo.cpp src\generated\gen_gunmods.cpp
set RC=%ERRORLEVEL%

echo.
if "%RC%"=="0" (
    echo ============ BUILD OK ============
    echo Output: %~dp0..\build\gunlab_gcc.exe
    echo.
    echo To run:  build\gunlab_gcc.exe
) else (
    echo ============ BUILD FAILED ^(exit %RC%^) ============
)

echo.
pause
endlocal
