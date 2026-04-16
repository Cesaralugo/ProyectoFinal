# ============================================================================
# audio_rpi/latency_testing/Makefile
#
# Self-contained build for:
#   bin/gui_engine.exe        - Native C Win32 GUI (original, lightweight)
#   bin/gui_engine_cpp.exe    - C++ ImGui GUI (full features, sub-10ms)
#   bin/latency_test.exe      - Per-batch latency measurement
#   bin/latency_benchmark.exe - Rapid profiling with histogram output
#
# Usage:
#   cd audio_rpi/latency_testing
#   make              # build C GUI + latency tools
#   make gui          # build only the original C GUI
#   make gui_cpp      # build the full-featured C++ ImGui GUI (needs CMake)
#   make tools        # build only the latency tools
#   make clean        # remove build artifacts (bin/ + c_gui/build/)
#
# Requirements:
#   - MinGW-w64 (gcc / g++) on PATH
#   - CMake 3.16+  (for gui_cpp target only)
# ============================================================================

CC      = gcc
CFLAGS  = -Wall -O2 -std=c11

# Output directory
BIN     = bin

# ── Original C GUI sources ────────────────────────────────────────────────────
GUI_SRC = c_gui/gui_main.c \
          c_gui/effect_manager.c \
          c_gui/visualizer.c \
          c_gui/socket_client.c

GUI_LIBS  = -lws2_32 -lgdi32 -lcomctl32
GUI_FLAGS = -mwindows   # suppress console window for the GUI

# ── Latency tool sources ─────────────────────────────────────────────────────
TEST_SRC  = latency_tools/latency_test.c
BENCH_SRC = latency_tools/latency_benchmark.c
TOOL_LIBS = -lws2_32 -lm

# ── Targets ──────────────────────────────────────────────────────────────────
.PHONY: all gui gui_cpp tools clean

all: gui tools

gui: $(BIN)/gui_engine.exe

tools: $(BIN)/latency_test.exe $(BIN)/latency_benchmark.exe

# C++ ImGui GUI – built via CMake; run_gui.bat automates this on Windows
gui_cpp:
	if not exist c_gui\build mkdir c_gui\build
	cd c_gui\build && cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release && mingw32-make -j4

$(BIN):
	if not exist $(BIN) mkdir $(BIN)

$(BIN)/gui_engine.exe: $(GUI_SRC) | $(BIN)
	$(CC) $(CFLAGS) $(GUI_FLAGS) -Ic_gui -o $@ $(GUI_SRC) $(GUI_LIBS)

$(BIN)/latency_test.exe: $(TEST_SRC) | $(BIN)
	$(CC) $(CFLAGS) -Ilatency_tools -o $@ $(TEST_SRC) $(TOOL_LIBS)

$(BIN)/latency_benchmark.exe: $(BENCH_SRC) | $(BIN)
	$(CC) $(CFLAGS) -Ilatency_tools -o $@ $(BENCH_SRC) $(TOOL_LIBS)

clean:
	if exist $(BIN) rmdir /S /Q $(BIN)
	if exist c_gui\build rmdir /S /Q c_gui\build
