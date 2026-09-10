@echo off
REM ============================================================================
REM  scripts\build_msvc.bat - Build gunlab with MSVC (Visual Studio)
REM  (ASCII only on purpose: cmd.exe uses the OEM codepage, not UTF-8)
REM ============================================================================
setlocal
cd /d "%~dp0.."          REM switch to the project root

set "VS=C:\Program Files\Microsoft Visual Studio\18\Community"
if not exist "%VS%\VC\Auxiliary\Build\vcvars64.bat" (
    echo [ERROR] vcvars64.bat not found. Edit the VS path in this script.
    echo         Current: %VS%
    pause
    exit /b 1
)

echo [1/3] Setting up MSVC environment...
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars64.bat failed.
    pause
    exit /b 1
)

echo [2/3] Creating build directory...
if not exist build mkdir build

echo [3/3] Compiling src\gunlab.cpp + src\gun_data.cpp ...
REM /utf-8 is REQUIRED: the sources are UTF-8, but MSVC defaults to the
REM system codepage (936 on Chinese Windows), which mangles Chinese literals.
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /Isrc ^
   /Fe:build\gunlab_msvc.exe /Fo:build\ ^
   src\gunlab.cpp src\gun_data.cpp ^
   src\generated\gen_guns.cpp src\generated\gen_ammo.cpp src\generated\gen_gunmods.cpp
set RC=%ERRORLEVEL%

echo.
if "%RC%"=="0" (
    echo ============ BUILD OK ============
    echo Output: %~dp0..\build\gunlab_msvc.exe
    echo.
    echo To run:  build\gunlab_msvc.exe
) else (
    echo ============ BUILD FAILED ^(exit %RC%^) ============
)

echo.
pause
endlocal
