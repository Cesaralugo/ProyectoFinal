@echo off
setlocal
REM ============================================================
REM  run_gui.bat  –  Build and launch the C++ ImGui GUI
REM  (run from the repository root OR from audio_rpi\latency_testing)
REM ============================================================

:: Locate the latency_testing folder relative to this script
set SCRIPT_DIR=%~dp0
set LT_DIR=%SCRIPT_DIR%audio_rpi\latency_testing

:: If the script is already inside latency_testing, adjust the path
if exist "%SCRIPT_DIR%c_gui\CMakeLists.txt" (
    set LT_DIR=%SCRIPT_DIR%
)

echo.
echo  ===============================================
echo    MultiFX Processor  –  C++ GUI (ImGui)
echo    Latency ^< 10 ms  •  OpenGL3 •  GLFW3
echo  ===============================================
echo.

:: ── Check for cmake ─────────────────────────────────────────
where cmake >NUL 2>&1
if errorlevel 1 (
    echo  [ERROR] cmake not found on PATH.
    echo  Install CMake from https://cmake.org/download/ and add it to PATH.
    pause
    exit /b 1
)

:: ── Check for MinGW make ─────────────────────────────────────
where mingw32-make >NUL 2>&1
if not errorlevel 1 (
    set MAKE_CMD=mingw32-make
) else (
    where make >NUL 2>&1
    if not errorlevel 1 (
        set MAKE_CMD=make
    ) else (
        echo  [ERROR] mingw32-make / make not found on PATH.
        echo  Install MinGW-w64 and add it to PATH.
        pause
        exit /b 1
    )
)

:: ── Configure ────────────────────────────────────────────────
echo  [1/3] Configuring CMake...
if not exist "%LT_DIR%c_gui\build" mkdir "%LT_DIR%c_gui\build"
cd /d "%LT_DIR%c_gui\build"
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo  [ERROR] CMake configure failed.
    pause
    exit /b 1
)

:: ── Build ────────────────────────────────────────────────────
echo.
echo  [2/3] Building (this may take a few minutes the first time)...
%MAKE_CMD% -j4
if errorlevel 1 (
    echo  [ERROR] Build failed.
    pause
    exit /b 1
)

:: ── Launch ───────────────────────────────────────────────────
echo.
echo  [3/3] Launching C++ GUI...
cd /d "%LT_DIR%"

:: Note: presets.json is created automatically by the GUI on first launch

start "" "bin\gui_engine_cpp.exe"

echo.
echo  GUI launched.  Close this window to exit.
pause
