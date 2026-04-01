#!/bin/bash

echo "Performance governor..."
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null

echo "Limpiando procesos viejos..."
pkill -f audio_engine
pkill -f main.py
sleep 0.5

cd audio_rpi
echo "Iniciando servidor C..."
taskset 0x1 ./audio_engine &   # sin sudo — setcap ya le dio los permisos RT
AUDIO_PID=$!

echo "Esperando socket..."
for i in $(seq 1 20); do
    [ -S /tmp/audio_socket ] && break
    sleep 0.1
done

if [ ! -S /tmp/audio_socket ]; then
    echo "ERROR: socket no apareció"
    kill $AUDIO_PID 2>/dev/null
    exit 1
fi
echo "Socket listo."

cd ../Interfaz
source ../env/bin/activate
echo "Iniciando interfaz..."
taskset 0x6 python main.py &

wait