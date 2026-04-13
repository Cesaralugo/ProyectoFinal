@echo off
setlocal

set PROJECT_DIR=%~dp0
set NI_DEVICE=%1
set NI_CHANNEL=%2
set NI_MODE=%3
if "%NI_DEVICE%"=="" set NI_DEVICE=Dev1
if "%NI_CHANNEL%"=="" set NI_CHANNEL=ai0
if "%NI_MODE%"=="" set NI_MODE=rse

:: GUI_MODE controls which front-end is launched:
::   cpp    – C++ ImGui GUI (gui_engine.exe) – lowest latency (default)
::   python – Python PyQt6 GUI               – legacy fallback
if "%GUI_MODE%"=="" set GUI_MODE=cpp

echo ============================================
echo   MultiFX Processor
echo   NI Device : %NI_DEVICE% / %NI_CHANNEL% (%NI_MODE%)
echo   GUI mode  : %GUI_MODE%
echo ============================================

:: Kill any leftover processes
taskkill /F /IM audio_engine.exe 2>NUL
taskkill /F /IM gui_engine.exe   2>NUL
taskkill /F /IM python.exe 2>NUL
timeout /t 1 >NUL

:: Start C NI feeder (much more reliable than Python)
echo [1/3] Starting C NI feeder...
cd "%PROJECT_DIR%audio_rpi"
start "C NI Feeder" ni_feeder.exe --device %NI_DEVICE% --channel %NI_CHANNEL% --mode %NI_MODE%

timeout /t 2 >NUL

:: Start audio engine
echo [2/3] Starting audio engine...
start "Audio Engine" audio_engine.exe

:: Wait for TCP socket to be ready
echo Waiting for audio engine TCP socket...
:wait_loop
timeout /t 1 >NUL
netstat -an | findstr "54321" | findstr "LISTENING" >NUL
if errorlevel 1 goto wait_loop
echo Socket ready.

:: Start GUI
echo [3/3] Starting GUI (%GUI_MODE% mode)...
if /I "%GUI_MODE%"=="cpp" (
    :: C++ ImGui GUI – sub-10ms latency, built-in mobile app receiver on port 5000
    cd "%PROJECT_DIR%audio_rpi\gui\build"
    if not exist gui_engine.exe (
        echo ERROR: gui_engine.exe not found.
        echo Build it first:
        echo   cd audio_rpi\gui
        echo   cmake -S . -B build -G "MinGW Makefiles"
        echo   cmake --build build --config Release
        pause
        goto cleanup
    )
    start "GUI" gui_engine.exe 127.0.0.1 54321
) else (
    :: Legacy Python GUI
    cd "%PROJECT_DIR%Interfaz"
    start "GUI" python main.py
)

echo.
echo All running. Close this window to stop everything.
pause

:cleanup
taskkill /F /IM audio_engine.exe 2>NUL
taskkill /F /IM gui_engine.exe   2>NUL
taskkill /F /IM python.exe 2>NUL