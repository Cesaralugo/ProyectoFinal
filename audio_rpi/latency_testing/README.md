# Latency Testing — Isolated Folder

This folder contains **all latency-testing and C-GUI work** in one place.
Deleting the entire `latency_testing/` directory restores the repository to
its original state — no other files are modified.

---

## Folder Structure

```
audio_rpi/latency_testing/
├── Makefile                       # Build everything (see § Build)
├── README.md                      # This file
├── bin/                           # Build output (created by make)
│   ├── gui_engine.exe
│   ├── latency_test.exe
│   └── latency_benchmark.exe
├── c_gui/                         # Native C Win32 GUI replacement
│   ├── gui_main.c                 # Win32 application & message loop
│   ├── effect_manager.c / .h      # Effect chain add/remove/reorder + JSON
│   ├── visualizer.c / .h          # Double-buffered GDI waveform renderer
│   └── socket_client.c / .h      # Non-blocking TCP client (port 54321)
└── latency_tools/                 # Latency measurement utilities
    ├── latency_monitor.h          # Shared QueryPerformanceCounter helpers
    ├── latency_test.c             # Live per-batch statistics
    └── latency_benchmark.c        # One-shot profiling with histogram
```

---

## Components

### C GUI (`c_gui/`)

A native Win32 replacement for the Python/PyQt6 interface.
It eliminates all Python overhead from the measurement path.

| File | Purpose |
|---|---|
| `gui_main.c` | Window, toolbar, message loop |
| `effect_manager.c/h` | Add/remove/reorder effects, serialise to JSON |
| `visualizer.c/h` | Real-time pre/post waveform display (GDI, double-buffered) |
| `socket_client.c/h` | Non-blocking TCP client; background receiver thread |

**Key design choices:**
- Win32 API only — no external GUI library
- JSON format is identical to the Python GUI so the audio engine needs no changes
- Socket sends are non-blocking (`FIONBIO`) to avoid stalling the UI thread
- Receiver thread runs at `THREAD_PRIORITY_ABOVE_NORMAL` to reduce scheduling jitter

### Latency Tools (`latency_tools/`)

| File | Purpose |
|---|---|
| `latency_monitor.h` | `QueryPerformanceCounter` clock, running stats, headroom calc |
| `latency_test.c` | Connects to engine, prints live min/max/avg/jitter every second |
| `latency_benchmark.c` | Collects N batches, prints histogram + P50/P95/P99 |

**Reference timing:**
- Sample rate: 44 100 Hz
- Batch size: 128 samples → **~2.9 ms** packet window
- Any single-batch latency > 2.9 ms is counted as an *overrun*

---

## Build

### Prerequisites

- **MinGW-w64** with `gcc` on your `PATH`
  (the same toolchain used by `audio_rpi/Makefile`)
- Windows SDK headers are bundled with MinGW — no extra installs needed

### Build all components

```bat
cd audio_rpi\latency_testing
make
```

Build individual targets:

```bat
make gui     :: only gui_engine.exe
make tools   :: only latency_test.exe + latency_benchmark.exe
make clean   :: delete bin\ directory
```

### Build output

All executables land in `bin\`:

```
bin\gui_engine.exe
bin\latency_test.exe
bin\latency_benchmark.exe
```

---

## Running the Tests

### 1 — Start the audio engine first

```bat
cd audio_rpi
make
.\audio_engine.exe
```

The engine opens TCP port **54321** and waits for a client.

### 2 — Live latency monitor

Opens a connection, prints a statistics table every second:

```bat
bin\latency_test.exe
:: or with explicit host/port:
bin\latency_test.exe 127.0.0.1 54321
```

Sample output:

```
Batches    Min(us)    Max(us)    Avg(us)    Jitter     Overruns
--------------------------------------------------------------
312        45.2       980.1      112.4      38.7       0
```

### 3 — One-shot benchmark with histogram

Collects a fixed number of batches then exits:

```bat
bin\latency_benchmark.exe            :: 1 000 batches
bin\latency_benchmark.exe 5000       :: 5 000 batches
```

Sample summary:

```
=== Benchmark Summary (1000 batches in 2.94 s) ===
  Min       :     41.3 us
  Max       :    1204.7 us
  Average   :    108.6 us
  Std dev   :     55.2 us
  P50       :     95.1 us
  P95       :    210.3 us
  P99       :    890.4 us
  Overruns  : 0 / 1000 (>2902 us)
  Throughput: 340 batches/s
```

### 4 — Native C GUI

Replaces `python Interfaz/main.py` with a lightweight Win32 window:

```bat
bin\gui_engine.exe
:: or with explicit host:
bin\gui_engine.exe 127.0.0.1 54321
```

The GUI auto-reconnects every 2 seconds if the engine is not yet running.

---

## Optional Integration with `run_all.bat`

To use the C GUI instead of Python, edit `run_all.bat` and replace:

```bat
start "GUI" python main.py
```

with:

```bat
start "GUI" "%PROJECT_DIR%audio_rpi\latency_testing\bin\gui_engine.exe"
```

The original line is untouched unless you edit it — the Python GUI continues
to work as before.

---

## Rollback

To revert **all** changes made by this folder:

```bat
rmdir /S /Q audio_rpi\latency_testing
```

That's it. No other file in the repository was modified.
