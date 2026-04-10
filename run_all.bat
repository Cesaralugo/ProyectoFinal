@echo off
setlocal

set PROJECT_DIR=%~dp0
set NI_DEVICE=%1
set NI_CHANNEL=%2
set NI_MODE=%3
if "%NI_DEVICE%"=="" set NI_DEVICE=Dev1
if "%NI_CHANNEL%"=="" set NI_CHANNEL=ai0
if "%NI_MODE%"=="" set NI_MODE=rse

echo ============================================
echo   MultiFX Processor
echo   NI Device : %NI_DEVICE% / %NI_CHANNEL% (%NI_MODE%)
echo ============================================

:: Kill any leftover processes
taskkill /F /IM audio_engine.exe 2>NUL
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
echo [3/3] Starting GUI...
cd "%PROJECT_DIR%Interfaz"
start "GUI" python main.py

echo.
echo All running. Close this window to stop everything.
pause

taskkill /F /IM audio_engine.exe 2>NUL
taskkill /F /IM python.exe 2>NUL