# Proyecto Final

Este proyecto contiene una interfaz en **PyQt6**, procesamiento de audio y comunicación entre diferentes módulos del sistema.

## Requisitos
Python 3
pip
make


Dependencias de Python:

PyQt6
numpy
zeroconf

## Instalación

Entrar a la carpeta de la interfaz:

```
cd Interfaz
```

Crear el entorno virtual:

```
python3 -m venv venv
```

Activar el entorno:

Linux 

```
source venv/bin/activate
```

Instalar dependencias:

```
pip install -r requirements.txt
```

## Compilar audio (IMPORTANTE)

Cada vez que se haga un **git pull**, es necesario recompilar el módulo de audio.

Entrar a la carpeta:

```
cd audio_rpi
```

y ejecutar:

```
make
```

## Ejecutar todo el sistema

Para correr todos los módulos del proyecto se usa el script:

```
run_all.sh
```

La primera vez hay que darle permisos de ejecución:

```
chmod +x run_all.sh
```

Luego se ejecuta con:
./run_all.sh


## Notas

Si se actualiza el repositorio con `git pull`, se recomienda volver a ejecutar `make` en `audio_rpi`.
El entorno virtual debe activarse antes de ejecutar la interfaz. Leer requirements.txt

---

## Módulo de adquisición ADC

Este módulo captura señales analógicas a ~44 kHz y las transmite por Serial en binario para su visualización y análisis espectral en Python. Compatible con ESP32, Arduino Mega 2560 y Arduino Uno.

### Dependencias Python

```
pip install pyserial numpy matplotlib PyQt6 pyqtgraph sounddevice
```

`sounddevice` es opcional — solo se necesita para la reproducción de audio en tiempo real en `monitor.py`.

---

### ESP32 (WROOM / WROVER)

**Archivo Arduino:** `Acquisition_ESP32.ino` (v11)

#### Wiring

| Pin | Conexión |
|-----|----------|
| GPIO36 (VP / ADC1_CH0) | Señal de entrada (máx. 3.3 V) |
| GND | GND |

Recomendado: resistencia de 1 kΩ en serie + condensador de 100 nF a GND en el pin de entrada.

#### Configuración IDE Arduino

| Parámetro | Valor |
|-----------|-------|
| Board | ESP32 Dev Module |
| CPU Frequency | 240 MHz |
| Upload Speed | 921600 |
| Core requerido | arduino-esp32 3.x (probado en 3.3.7) |

#### Parámetros del stream

| Parámetro | Valor |
|-----------|-------|
| Baud rate | 460800 |
| Frecuencia de muestreo (salida) | ~22 039 Hz (ISR a 44 077 Hz con decimación 2:1) |
| Muestras por paquete | 128 |
| Resolución ADC | 12 bits (0 – 4095) |
| Tamaño de paquete | 260 bytes |
| Sync word | `0xAA 0x55 0xFF 0x00` |
| Voltaje de referencia | 3.3 V |

> **VERBOSE_BOOT:** El sketch tiene `#define VERBOSE_BOOT 1` por defecto para ver el log de arranque. Esto corrompe el stream binario. Cambiar a `0` y re-subir antes de usar `monitor.py` o `diagnose.py`.

#### Comandos

```bash
# Diagnóstico raw (usar siempre primero)
python diagnose.py --port /dev/ttyUSB0 --baud 460800

# Monitor gráfico en tiempo real
python monitor.py --port /dev/ttyUSB0 --baud 460800 --fs 22039 --vref 3.3 --esp32

# Captura por lotes + gráfico FFT
python acquisition.py --port /dev/ttyUSB0 --baud 460800 --fs 22039 --bufsize 128 --vref 3.3
```

---

### Arduino Mega 2560

**Archivo Arduino:** `Acquisition.ino` (v2)

#### Wiring

| Pin | Conexión |
|-----|----------|
| A0 | Señal de entrada (máx. 5 V, impedancia fuente < 10 kΩ) |
| GND | GND |

Recomendado: condensador de 100 nF cerámico de A0 a GND.

#### Configuración IDE Arduino

| Parámetro | Valor |
|-----------|-------|
| Board | Arduino Mega or Mega 2560 |
| Processor | ATmega2560 |
| Upload Speed | 115200 |

#### Parámetros del stream

| Parámetro | Valor |
|-----------|-------|
| Baud rate | 1 000 000 |
| Frecuencia de muestreo | ~44 077 Hz (Timer1 overflow, prescaler /1) |
| Muestras por paquete | 256 |
| Resolución ADC | 10 bits (0 – 1023) |
| Tamaño de paquete | 516 bytes |
| Sync word | `0xAA 0x55 0xFF 0x00` |
| Voltaje de referencia | 5.0 V (AVcc) |

#### Comandos

```bash
python diagnose.py --port /dev/ttyUSB0 --baud 1000000

python monitor.py --port /dev/ttyUSB0 --baud 1000000 --fs 44077 --vref 5.0

python acquisition.py --port /dev/ttyUSB0 --baud 1000000 --fs 44077 --bufsize 256 --vref 5.0
```

---

### Arduino Uno (R3 / R4)

Usa el mismo ATmega328P que el Mega. El sketch `Acquisition.ino` compila y funciona sin cambios. La única diferencia es que el Uno tiene un único puerto Serial compartido con USB — no es posible usar `Serial.print` para depurar mientras se transmite el stream binario.

#### Wiring

Idéntico al Mega: señal en **A0**, GND en GND.

#### Configuración IDE Arduino

| Parámetro | Valor |
|-----------|-------|
| Board | Arduino Uno |
| Upload Speed | 115200 |

#### Parámetros del stream

Idénticos al Mega.

#### Comandos

```bash
python diagnose.py --port /dev/ttyUSB0 --baud 1000000

python monitor.py --port /dev/ttyUSB0 --baud 1000000 --fs 44077 --vref 5.0

python acquisition.py --port /dev/ttyUSB0 --baud 1000000 --fs 44077 --bufsize 256 --vref 5.0
```

---

### Referencia rápida de parámetros

| Plataforma | `--baud` | `--fs` | `--bufsize` | `--vref` | `--esp32` |
|------------|----------|--------|-------------|----------|-----------|
| ESP32 | 460800 | 22039 | 128 | 3.3 | ✓ requerido |
| Arduino Mega | 1000000 | 44077 | 256 | 5.0 | no usar |
| Arduino Uno | 1000000 | 44077 | 256 | 5.0 | no usar |

---

### Herramientas Python

#### `diagnose.py`

Inspector de stream raw. Usar siempre antes de `monitor.py` para confirmar que llegan bytes, que el sync word está presente y que los valores son plausibles.

```
python diagnose.py --port PUERTO [--baud BAUD] [--bytes N] [--timeout S]
```

| Argumento | Default | Descripción |
|-----------|---------|-------------|
| `--port` | requerido | Puerto serie (`/dev/ttyUSB0`, `COM3`, etc.) |
| `--baud` | 2000000 | Debe coincidir con `Serial.begin()` en el sketch |
| `--bytes` | 512 | Bytes a capturar |
| `--timeout` | 6.0 | Segundos máximos de espera |

#### `monitor.py`

Monitor gráfico en tiempo real: forma de onda, FFT e histograma de amplitud. Requiere PyQt6 y pyqtgraph.

```
python monitor.py --port PUERTO [--baud BAUD] [--fs HZ] [--vref V] [--esp32]
```

| Argumento | ESP32 | Mega / Uno |
|-----------|-------|------------|
| `--baud` | 460800 | 1000000 |
| `--fs` | 22039 | 44077 |
| `--vref` | 3.3 | 5.0 |
| `--esp32` | requerido | no usar |

#### `acquisition.py`

Captura N paquetes y genera un PNG con la forma de onda y el espectro FFT.

```
python acquisition.py --port PUERTO [--baud BAUD] [--fs HZ] [--bufsize N] [--packets N] [--vref V] [--live]
```

| Argumento | Default | Descripción |
|-----------|---------|-------------|
| `--baud` | 2000000 | Debe coincidir con el sketch |
| `--fs` | 44077 | Frecuencia de muestreo declarada |
| `--bufsize` | 512 | Muestras por paquete — **debe coincidir con el sketch** |
| `--packets` | 20 | Paquetes a capturar |
| `--vref` | 5.0 | Voltaje de referencia ADC |
| `--live` | flag | Modo FFT en tiempo real en lugar de captura por lotes |

---

### Solución de problemas

**0 bytes recibidos:** verificar que el sketch esté subido y corriendo, que el puerto sea el correcto (`/dev/ttyUSB0`, `/dev/ttyACM0`, etc.) y que Arduino IDE Serial Monitor u otro proceso no tenga el puerto abierto.

**Valores todos cero (ESP32):** asegurarse de que `VERBOSE_BOOT` sea `0` en el sketch. Con `1`, el texto ASCII corrompe el stream binario y el parser no encuentra muestras válidas.

**`timerBegin` retorna NULL (ESP32):** el sketch v11 reintenta automáticamente hasta 5 veces. Si persiste, verificar que la board seleccionada sea "ESP32 Dev Module" y que la frecuencia de CPU esté en 240 MHz.

**Sync word no encontrado:** baud rate incorrecto. Usar la tabla de referencia rápida y confirmar que `--baud` coincide exactamente con `Serial.begin()` en el sketch.

**Pattern `00 00 80` en el hex dump (ESP32):** el chip USB-UART (CH340 / CP2102) está generando errores de framing porque el driver de Linux no soporta el baud rate solicitado. Usar 460800 (no 921600 ni 2000000) con el ESP32.
