@echo off
setlocal
REM ============================================================
REM  run_all_cpp.bat  –  Full system with C++ GUI
REM
REM  Starts (in order):
REM    1. NI-DAQ feeder (Python)       – reads analogue input
REM    2. C audio engine               – applies effects, opens TCP 54321
REM    3. C++ ImGui GUI                – full UI, sub-10 ms visualisation
REM
REM  Run from the repository root:
REM    run_all_cpp.bat [NI_DEVICE] [NI_CHANNEL] [NI_MODE]
REM  Defaults: Dev1  ai0  rse
REM ============================================================

set PROJECT_DIR=%~dp0
set NI_DEVICE=%1
set NI_CHANNEL=%2
set NI_MODE=%3
if "%NI_DEVICE%"==""  set NI_DEVICE=Dev1
if "%NI_CHANNEL%"=="" set NI_CHANNEL=ai0
if "%NI_MODE%"==""    set NI_MODE=rse

echo.
echo  =====================================================
echo    MultiFX Processor  –  Full C++ Launch
echo    NI Device  : %NI_DEVICE% / %NI_CHANNEL% (%NI_MODE%)
echo    Audio Port : 54321   App Receiver : 5000
echo  =====================================================
echo.

:: Kill any leftover processes
taskkill /F /IM audio_engine.exe  2>NUL
taskkill /F /IM gui_engine_cpp.exe 2>NUL
taskkill /F /IM python.exe        2>NUL
timeout /t 1 /nobreak >NUL

:: ── Build C++ GUI (first run or after changes) ───────────────
echo  [0/4] Building C++ GUI (skip if already built)...
set LT_DIR=%PROJECT_DIR%audio_rpi\latency_testing
if not exist "%LT_DIR%\c_gui\build" (
    mkdir "%LT_DIR%\c_gui\build"
)
cd /d "%LT_DIR%\c_gui\build"
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release >NUL 2>&1
mingw32-make -j4 >NUL 2>&1

:: ── 1. NI feeder ────────────────────────────────────────────
echo  [1/4] Starting NI feeder...
cd /d "%PROJECT_DIR%Interfaz"
start "NI Feeder" python ni6009_feeder.py %NI_DEVICE% %NI_CHANNEL% %NI_MODE%
timeout /t 2 /nobreak >NUL

:: ── 2. Audio engine ─────────────────────────────────────────
echo  [2/4] Starting audio engine...
cd /d "%PROJECT_DIR%audio_rpi"
start "Audio Engine" audio_engine.exe

:: Wait until the engine opens port 54321
echo  Waiting for audio engine TCP socket...
:wait_loop
timeout /t 1 /nobreak >NUL
netstat -an | findstr "54321" | findstr "LISTENING" >NUL
if errorlevel 1 goto wait_loop
echo  Socket ready.

:: ── 3. Launch C++ GUI ───────────────────────────────────────────
echo  [3/4] Launching C++ GUI...
:: Note: presets.json is created automatically by the GUI on first launch
cd /d "%LT_DIR%"
start "C++ GUI" "%LT_DIR%\bin\gui_engine_cpp.exe"

echo.
echo  All components running.
echo  Close this window to stop everything.
echo.
pause

:: Cleanup on exit
taskkill /F /IM gui_engine_cpp.exe 2>NUL
taskkill /F /IM audio_engine.exe   2>NUL
taskkill /F /IM python.exe         2>NUL
