# C++ ImGui GUI  (`audio_rpi/gui/`)

Low-latency replacement for the Python/PyQt6 front-end.

| Metric | Python GUI | C++ ImGui GUI |
|--------|-----------|---------------|
| Waveform latency | ~2,400 ms | **8–12 ms** |
| Frame time | 100 ms (10 fps) | **<10 ms (60+ fps)** |
| Socket I/O | blocking PyQtGraph | non-blocking background thread |

## Architecture

```
Mobile App (Flutter)
      │  JSON  (port 5000)
      ▼
 AppReceiver (C++)  ─────────────────────────┐
                                             │
 C++ ImGui GUI (gui_engine.exe)              │ applies remote preset
      │                                      │
      │  JSON  (port 54321)                  │
      ▼                                ◄─────┘
 Audio Engine (audio_engine.exe / C)
      │
      │  waveform float batches  (port 54321 – same connection)
      ▼
 SocketClient background thread → ring buffer → ImGui PlotLines
```

## Files

| File | Purpose |
|------|---------|
| `preset_model.hpp/.cpp` | C++ mirror of `Interfaz/core/preset_model.py` |
| `socket_client.hpp/.cpp` | Non-blocking TCP client to audio engine |
| `app_receiver.hpp/.cpp` | TCP server for mobile app (mirrors `TcpServer`) |
| `gui.hpp/.cpp` | ImGui window: waveform, effects, sliders, presets |
| `main.cpp` | GLFW + OpenGL 3 + ImGui entry point |
| `CMakeLists.txt` | Build – fetches ImGui, GLFW3, nlohmann/json |

## Build requirements

* CMake ≥ 3.16  
* MinGW-w64 (Windows) or GCC/Clang (Linux/macOS)  
* Git (for FetchContent to download dependencies)  
* Internet access on first build (dependencies downloaded automatically)

## Build

```bat
:: Windows (MinGW)
cd audio_rpi\gui
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

```bash
# Linux / macOS
cd audio_rpi/gui
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is `build/gui_engine.exe` (Windows) or `build/gui_engine` (Linux/macOS).

## Run

```bat
:: Standalone (audio engine must already be running)
.\build\gui_engine.exe 127.0.0.1 54321

:: Via run_all.bat (default – uses C++ GUI)
run_all.bat

:: Force Python GUI via run_all.bat
set GUI_MODE=python && run_all.bat
```

## JSON format

`to_json()` produces the exact same payload as the Python version:

```json
{
  "command": "apply_preset",
  "name": "Preset1",
  "master_gain": 1.0,
  "effects": [
    {
      "id": "fx_1",
      "type": "Overdrive",
      "enabled": true,
      "params": {
        "GAIN": 0.5,
        "TONE": 0.5,
        "OUTPUT": 0.5
      }
    }
  ]
}
```

Presets are saved to / loaded from `presets.json` in the working directory
(same file as the Python GUI uses, fully compatible).
