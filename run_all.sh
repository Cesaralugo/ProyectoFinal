#!/bin/bash

echo "Limpiando procesos viejos..."
pkill -f main    # tu servidor C
pkill -f main.py # la interfaz Python
sleep 0.5

# Ejecutable C (socket server)
cd ~/Proyecto_Final/audio_rpi
echo "Iniciando servidor C..."
./audio_engine & # ejecutable de C
sleep 0.5        # espera a que el socket se cree

# Activar virtual environment y correr Python GUI
cd ~/ProyectoFinal/Interfaz
echo "Activando virtual environment..."
source env/bin/activate # o| venv/bin/activate si usas venv

echo "Iniciando interfaz..."
python main.py &

# Mantener el script activo mientras corren todos
wait
