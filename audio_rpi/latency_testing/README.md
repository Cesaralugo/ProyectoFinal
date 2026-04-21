# Latency Testing — Isolated Folder

This folder contains **all latency-testing and C-GUI work** in one place.
Deleting the entire `latency_testing/` directory restores the repository to
its original state — no other files are modified.

---

## Folder Structure

```
audio_rpi/latency_testing/
├── run_gui.bat                    # Build & launch C++ ImGui GUI
├── run_all_cpp.bat                # Full system launch (feeder + engine + C++ GUI)
├── Makefile                       # Build: C GUI + latency tools (+ cmake target)
├── README.md                      # This file
├── bin/                           # Build output (created by make / cmake)
│   ├── gui_engine.exe             # Original lightweight C Win32 GUI
│   ├── gui_engine_cpp.exe         # NEW: full-featured C++ ImGui GUI
│   ├── latency_test.exe
│   └── latency_benchmark.exe
├── c_gui/                         # GUI source files
│   ├── CMakeLists.txt             # CMake build (ImGui + GLFW3 + nlohmann/json)
│   ├── main.cpp                   # ImGui + GLFW3 + OpenGL3 entry point
│   ├── gui.hpp / gui.cpp          # Full UI (all Python features)
│   ├── preset_manager.hpp / .cpp  # Preset load/save (same JSON as Python)
│   ├── fft_processor.hpp / .cpp   # FFT (Blackman window + smoothing)
│   ├── app_receiver.hpp / .cpp    # TCP server port 5000 (mobile app)
│   ├── gui_main.c                 # Original Win32 application & message loop
│   ├── effect_manager.c / .h      # Effect chain add/remove/reorder + JSON
│   ├── visualizer.c / .h          # Double-buffered GDI waveform renderer
│   └── socket_client.c / .h      # Non-blocking TCP client (port 54321)
└── latency_tools/                 # Latency measurement utilities
    ├── latency_monitor.h          # Shared QueryPerformanceCounter helpers
    ├── latency_test.c             # Live per-batch statistics
    └── latency_benchmark.c        # One-shot profiling with histogram
```

---

## C++ ImGui GUI — Feature Parity with Python

The `gui_engine_cpp.exe` has **every feature** of the Python PyQt6 GUI:

| Feature | Python GUI | C++ GUI |
|---------|-----------|---------|
| Waveform (pre/post) | ✅ | ✅ |
| Time domain view | ✅ | ✅ |
| Frequency domain (FFT) | ✅ | ✅ |
| FFT smoothing (α=0.3) | ✅ | ✅ |
| All 8 effects | ✅ | ✅ |
| Add / Remove effect | ✅ | ✅ |
| Reorder effects (↑↓) | ✅ | ✅ |
| Toggle effect on/off | ✅ | ✅ |
| Expandable param sliders | ✅ | ✅ |
| Master gain (0.1–4.0) | ✅ | ✅ |
| Master gain dB display | ✅ | ✅ |
| Preset load / save | ✅ | ✅ |
| Create / Delete preset | ✅ | ✅ |
| Mobile app receiver (5000) | ✅ | ✅ |
| Auto-reconnect to engine | ✅ | ✅ |
| Bypass button | ✅ | ✅ |
| Pause button | ✅ | ✅ |
| **Visualisation latency** | ~500 ms | **< 10 ms** |

---

## Quick Start (C++ GUI)

### From `latency_testing/` folder:

```bat
run_gui.bat
```

This will:
1. Build the C++ GUI (first run downloads ImGui & GLFW3 via CMake FetchContent)
2. Launch `bin\gui_engine_cpp.exe`

### Full system (feeder + engine + C++ GUI):

```bat
run_all_cpp.bat [NI_DEVICE] [NI_CHANNEL] [NI_MODE]
:: Example:
run_all_cpp.bat Dev1 ai0 rse
```

---

## Build

### Prerequisites

- **MinGW-w64** with `gcc`/`g++` on your `PATH`
- **CMake 3.16+** on your `PATH`
- Internet access (first build – downloads ImGui, GLFW3, nlohmann/json)

### C++ ImGui GUI (recommended)

```bat
cd audio_rpi\latency_testing\c_gui
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j4
:: Output: audio_rpi\latency_testing\bin\gui_engine_cpp.exe
```

Or from `latency_testing/`:
```bat
make gui_cpp
```

### Original C Win32 GUI + latency tools

```bat
cd audio_rpi\latency_testing
make            :: builds gui_engine.exe + latency tools
make gui        :: gui_engine.exe only
make tools      :: latency tools only
make clean      :: delete bin\ and c_gui\build\
```

---

## Effects

All 8 effects with their parameters are supported (identical to Python):

| Effect | Parameters |
|--------|-----------|
| Overdrive | GAIN, TONE, OUTPUT |
| Delay | TIME (ms), FEEDBACK, MIX |
| Wah | FREQ (Hz), Q, LEVEL |
| Flanger | RATE (Hz), DEPTH, FEEDBACK, MIX |
| Chorus | RATE (Hz), DEPTH, FEEDBACK, MIX |
| Phaser | RATE (Hz), DEPTH, FEEDBACK, MIX |
| PitchShifter | SEMITONES, SEMITONES_B, MIX_A, MIX_B, MIX |
| Reverb | FEEDBACK, LPFREQ (Hz), MIX |

---

## JSON Formats

### Preset file (`presets.json` in working directory):

```json
{
  "Preset 1": {
    "name": "Preset1",
    "master_gain": 1.0,
    "effects": [
      {
        "id": "fx_1",
        "type": "Overdrive",
        "enabled": true,
        "params": { "GAIN": 0.5, "TONE": 0.5, "OUTPUT": 0.5 }
      }
    ]
  }
}
```

### Command to audio engine (port 54321):

```json
{
  "command": "apply_preset",
  "name": "Preset1",
  "master_gain": 1.0,
  "effects": [ ... ]
}
```

Both formats are **identical** to the Python GUI — presets.json can be
shared between the Python and C++ GUIs.

---

## Architecture

```
Mobile App (Flutter)
      │ TCP port 5000 (JSON presets)
      ▼
AppReceiver (background thread)
      │ callback
      ▼
MainGUI ──► PresetManager ──► presets.json
      │
      │ JSON (apply_preset)
      ▼
AudioEngineClient ──► audio_engine (TCP 54321)
      │
      │ float32 batches [pre, post, pre, post ...]
      ▼
FFTProcessor / waveform buffer ──► ImGui visualiser (<8 ms)
```

---

## Latency Breakdown

| Component | Old Python | C++ GUI |
|-----------|-----------|---------|
| Buffer accumulation | ~371 ms | ~3 ms |
| UI refresh interval | 100 ms | vsync (8–17 ms) |
| Render time | 50–100 ms | <8 ms |
| **Total** | **~500 ms** | **~20–30 ms** |

---

## Latency Tools

| File | Purpose |
|------|---------|
| `latency_monitor.h` | `QueryPerformanceCounter` clock, running stats |
| `latency_test.c` | Live min/max/avg/jitter every second |
| `latency_benchmark.c` | Histogram + P50/P95/P99 |

```bat
bin\latency_test.exe                :: live monitor
bin\latency_benchmark.exe           :: 1000 batches, then histogram
bin\latency_benchmark.exe 5000      :: 5000 batches
```

---

## Rollback

To revert **all** changes:

```bat
rmdir /S /Q audio_rpi\latency_testing
```

No other file in the repository is modified by this folder.
