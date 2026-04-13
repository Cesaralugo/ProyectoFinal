# ============================================================================
# audio_rpi/latency_testing/Makefile
#
# Self-contained build for:
#   bin/gui_engine.exe       - Native C Win32 GUI replacement
#   bin/latency_test.exe     - Per-batch latency measurement
#   bin/latency_benchmark.exe - Rapid profiling with histogram output
#
# Usage:
#   cd audio_rpi/latency_testing
#   make            # build everything
#   make gui        # build only the C GUI
#   make tools      # build only the latency tools
#   make clean      # remove build artifacts
#
# Requirements:
#   MinGW-w64 (gcc / g++) on PATH
# ============================================================================

CC      = gcc
CFLAGS  = -Wall -O2 -std=c11

# Output directory
BIN     = bin

# ── GUI sources ──────────────────────────────────────────────────────────────
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
.PHONY: all gui tools clean

all: gui tools

gui: $(BIN)/gui_engine.exe

tools: $(BIN)/latency_test.exe $(BIN)/latency_benchmark.exe

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
