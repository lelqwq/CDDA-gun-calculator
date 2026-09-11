@echo off
REM ============================================================================
REM  scripts\build_gui.bat - Build the ImGui GUI version with MSYS2 MinGW-w64
REM ----------------------------------------------------------------------------
REM  The GUI needs SDL3, and the only SDL3 on this machine is the MinGW build
REM  in C:\msys64\mingw64 -- MSVC cannot link it. So the GUI must be built with
REM  g++, not with the Visual Studio generator.
REM  (ASCII only on purpose: cmd.exe uses the OEM codepage, not UTF-8)
REM ============================================================================
setlocal
REM switch to the project root (%~dp0 ends with a backslash)
cd /d "%~dp0.."

set "MINGW=C:\msys64\mingw64"
if not exist "%MINGW%\bin\g++.exe" (
    echo [ERROR] g++.exe not found at %MINGW%\bin
    echo         Edit the MINGW path in this script.
    pause
    exit /b 1
)

REM The compiler and the resulting exe both need these DLLs on PATH
set "PATH=%MINGW%\bin;%PATH%"

echo [1/3] Configuring (MinGW Makefiles + msys64 prefix)...
cmake -B build-gui -S . -G "MinGW Makefiles" ^
      -DCMAKE_PREFIX_PATH=%MINGW:\=/% ^
      -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto fail

echo [2/3] Building...
cmake --build build-gui
if errorlevel 1 goto fail

echo [3/3] Copying MinGW runtime DLLs next to the exe...
REM  -static only covers our own exe. SDL3.dll is a msys64 shared library and
REM  still drags libiconv-2.dll and friends behind it -- without those the exe
REM  dies with "libiconv-2.dll not found". The script walks the PE import tables
REM  recursively, so it keeps working when msys64 updates SDL3's dependencies.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0copy_gui_deps.ps1" ^
           -Exe "%~dp0..\build-gui\gunlab_gui.exe" -Dest "%~dp0..\build-gui" ^
           -Mingw "%MINGW%"
if errorlevel 1 goto fail

echo.
echo ============ BUILD OK ============
echo Output: %~dp0..\build-gui\gunlab_gui.exe
echo.
echo To run:  build-gui\gunlab_gui.exe
echo.
pause
endlocal
exit /b 0

:fail
echo.
echo ============ BUILD FAILED ============
echo.
pause
endlocal
exit /b 1
