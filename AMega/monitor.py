#!/usr/bin/env python3
"""
monitor.py  —  PyQt6 live monitor + audio playback for Arduino Mega ADC @ 44 kHz
══════════════════════════════════════════════════════════════════════════════════
Install:
    pip install PyQt6 pyqtgraph pyserial numpy sounddevice

Usage:
    python monitor.py --port /dev/ttyUSB0
    python monitor.py --port COM4 --baud 2000000 --vref 3.3
"""

import argparse
import sys
import time
import threading
import queue
import numpy as np
import serial

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QComboBox, QSpinBox, QDoubleSpinBox,
    QStatusBar, QFrame, QSlider
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QObject
from PyQt6.QtGui import QColor

import pyqtgraph as pg

# ── Optional audio ─────────────────────────────────────────────────────────────
try:
    import sounddevice as sd
    AUDIO_AVAILABLE = True
except ImportError:
    AUDIO_AVAILABLE = False

# ── Protocol ───────────────────────────────────────────────────────────────────
SYNC_WORD   = bytes([0xAA, 0x55, 0xFF, 0x00])
BUFFER_SIZE = 128   # must match PACKET_SAMPLES in the Arduino sketch
ADC_BITS    = 12    # ESP32 ADC is 12-bit (0–4095)

# ── Colour palette ─────────────────────────────────────────────────────────────
CLR_BG      = "#1e1e2e"
CLR_SURFACE = "#2a2a3e"
CLR_BORDER  = "#44475a"
CLR_ACCENT  = "#bd93f9"
CLR_GREEN   = "#50fa7b"
CLR_ORANGE  = "#ffb86c"
CLR_RED     = "#ff5555"
CLR_CYAN    = "#8be9fd"
CLR_FG      = "#f8f8f2"
CLR_MUTED   = "#6272a4"


# ══════════════════════════════════════════════════════════════════════════════
#  Audio engine — dedicated thread feeds sounddevice output stream
# ══════════════════════════════════════════════════════════════════════════════
class AudioEngine:
    """
    Pulls float32 mono samples from `audio_queue` and writes them to a
    sounddevice OutputStream.

    DC offset removal: the Arduino ADC idles around Vref/2 ≈ 2.5 V.
    A single-pole IIR high-pass (α = 0.995, –3 dB ≈ 35 Hz) removes DC
    before playback so the speaker doesn't thud on enable.

    Volume is a simple gain in [0.0, 1.0].
    """

    def __init__(self, fs: float = 44_077):
        self.fs          = fs
        self.volume      = 0.5
        self._running    = False
        self._stream     = None
        self.audio_queue: queue.Queue = queue.Queue(maxsize=64)

        # DC-blocking IIR state
        self._hp_x_prev = 0.0
        self._hp_y_prev = 0.0
        self._hp_alpha  = 0.995      # pole → –3 dB ≈ 35 Hz @ 44 kHz

    # ── Public API ────────────────────────────────────────────────────────────
    def push(self, raw_adc: np.ndarray, esp32: bool = False, millivolt: bool = False):
        """Normalise ADC codes → float, DC-block, enqueue for playback."""
        if not self._running:
            return
        if esp32:
            # 12-bit codes 0–4095, midpoint = 2048
            samples = (raw_adc.astype(np.float32) - 2048.0) / 2048.0
        elif millivolt:
            samples = (raw_adc.astype(np.float32) - 1650.0) / 1650.0
        else:
            # 10-bit AVR codes 0–1023, midpoint = 512
            samples = (raw_adc.astype(np.float32) - 512.0) / 512.0
        # DC-blocking: y[n] = α * (y[n-1] + x[n] − x[n-1])
        out    = np.empty_like(samples)
        xp     = self._hp_x_prev
        yp     = self._hp_y_prev
        a      = self._hp_alpha
        for i, x in enumerate(samples):
            y = a * (yp + x - xp)
            out[i] = y
            xp, yp = x, y
        self._hp_x_prev = xp
        self._hp_y_prev = yp
        try:
            self.audio_queue.put_nowait(out)
        except queue.Full:
            pass

    def start(self):
        if not AUDIO_AVAILABLE:
            return
        self._running = True
        self._stream  = sd.OutputStream(
            samplerate=int(self.fs),
            channels=1,
            dtype='float32',
            blocksize=BUFFER_SIZE,
            callback=self._callback,
        )
        self._stream.start()

    def stop(self):
        self._running = False
        if self._stream:
            try:
                self._stream.stop()
                self._stream.close()
            except Exception:
                pass
            self._stream = None
        while not self.audio_queue.empty():
            try:
                self.audio_queue.get_nowait()
            except queue.Empty:
                break

    def set_volume(self, v: float):
        self.volume = max(0.0, min(1.0, v))

    # ── sounddevice callback (audio thread) ───────────────────────────────────
    def _callback(self, outdata: np.ndarray, frames: int, time_info, status):
        out     = np.zeros(frames, dtype=np.float32)
        written = 0
        while written < frames:
            try:
                chunk = self.audio_queue.get_nowait()
            except queue.Empty:
                break
            take = min(len(chunk), frames - written)
            out[written:written + take] = chunk[:take]
            written += take
            if take < len(chunk):
                try:
                    self.audio_queue.put_nowait(chunk[take:])
                except queue.Full:
                    pass
        outdata[:, 0] = out * self.volume


# ══════════════════════════════════════════════════════════════════════════════
#  Serial reader
# ══════════════════════════════════════════════════════════════════════════════
class SerialReader(QObject):
    error = pyqtSignal(str)

    def __init__(self, port: str, baud: int,
                 pkt_queue: queue.Queue,
                 audio_engine=None,
                 esp32: bool = False):
        super().__init__()
        self.port         = port
        self.baud         = baud
        self.queue        = pkt_queue
        self.audio        = audio_engine
        self.millivolt    = esp32      # reuse field name for compat
        self._stop        = threading.Event()
        self._thread      = threading.Thread(target=self._run, daemon=True)
        self.ser          = None
        self.packets_rx   = 0
        self.packets_lost = 0

    def start(self):
        self._stop.clear()
        self._thread.start()

    def stop(self):
        self._stop.set()
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass

    def _find_sync(self) -> bool:
        window = bytearray(len(SYNC_WORD))
        for _ in range(BUFFER_SIZE * 8):
            if self._stop.is_set():
                return False
            b = self.ser.read(1)
            if not b:
                return False
            window = window[1:] + bytearray(b)
            if bytes(window) == SYNC_WORD:
                return True
        return False

    def _run(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=2)
        except serial.SerialException as e:
            self.error.emit(str(e))
            return
        time.sleep(1.5)
        self.ser.reset_input_buffer()
        while not self._stop.is_set():
            if not self._find_sync():
                self.packets_lost += 1
                continue
            raw = self.ser.read(BUFFER_SIZE * 2)
            if len(raw) < BUFFER_SIZE * 2:
                self.packets_lost += 1
                continue
            samples = np.frombuffer(raw, dtype='<u2').astype(np.float32)
            self.packets_rx += 1
            if self.audio:
                self.audio.push(samples, esp32=self.millivolt, millivolt=False)
            try:
                self.queue.put_nowait(samples)
            except queue.Full:
                pass


# ══════════════════════════════════════════════════════════════════════════════
#  Main window
# ══════════════════════════════════════════════════════════════════════════════
class MainWindow(QMainWindow):
    def __init__(self, args):
        super().__init__()
        self.args  = args
        self.fs    = args.fs
        self.vref  = args.vref
        self.esp32     = getattr(args, 'esp32', False)
        self.millivolt = False   # legacy, kept for compat
        self.queue = queue.Queue(maxsize=8)
        self.reader = None
        self.audio  = AudioEngine(fs=self.fs) if AUDIO_AVAILABLE else None

        self.ring_len      = 4096   # ~185 ms at 22 kHz — enough for time display
        self.ring          = np.zeros(self.ring_len, dtype=np.float32)
        self.fft_size      = 2048   # FFT points → 10.7 Hz/bin at 22 kHz
        self.t_start       = None
        self.total_samples = 0

        self._build_ui()
        self._apply_style()

        self.timer = QTimer()
        self.timer.setInterval(33)
        self.timer.timeout.connect(self._update_plots)

        if args.port:
            self._connect(args.port)

    # ── UI ────────────────────────────────────────────────────────────────────
    def _build_ui(self):
        self.setWindowTitle("Arduino / ESP32 — ADC Live Monitor  |  44 kHz")
        self.resize(1200, 840)

        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(10, 8, 10, 8)
        root.setSpacing(5)

        # ── Row 1: connection controls ─────────────────────────────────────────
        tb1 = QHBoxLayout()
        root.addLayout(tb1)

        tb1.addWidget(QLabel("Port:"))
        self.port_combo = QComboBox()
        self.port_combo.setEditable(True)
        self.port_combo.setMinimumWidth(160)
        self.port_combo.addItem(self.args.port or "")
        self._populate_ports()
        tb1.addWidget(self.port_combo)

        tb1.addWidget(QLabel("Baud:"))
        self.baud_combo = QComboBox()
        for b in [115200, 230400, 460800, 921600, 500000, 1000000, 2000000]:
            self.baud_combo.addItem(str(b))
        self.baud_combo.setCurrentText(str(self.args.baud))
        tb1.addWidget(self.baud_combo)

        tb1.addWidget(QLabel("Vref (V):"))
        self.vref_spin = QDoubleSpinBox()
        self.vref_spin.setRange(1.0, 5.5)
        self.vref_spin.setSingleStep(0.1)
        self.vref_spin.setValue(self.vref)
        self.vref_spin.valueChanged.connect(self._on_vref_changed)
        tb1.addWidget(self.vref_spin)

        tb1.addWidget(QLabel("Time window:"))
        self.window_spin = QSpinBox()
        self.window_spin.setRange(5, 500)
        self.window_spin.setValue(50)
        self.window_spin.setSuffix(" ms")
        tb1.addWidget(self.window_spin)

        tb1.addSpacing(10)

        self.btn_connect = QPushButton("▶  Connect")
        self.btn_connect.setFixedWidth(120)
        self.btn_connect.clicked.connect(self._on_connect_clicked)
        tb1.addWidget(self.btn_connect)

        self.btn_freeze = QPushButton("❚❚  Freeze")
        self.btn_freeze.setFixedWidth(110)
        self.btn_freeze.setCheckable(True)
        self.btn_freeze.clicked.connect(self._on_freeze_clicked)
        tb1.addWidget(self.btn_freeze)

        tb1.addStretch()

        # ── Row 2: audio controls ──────────────────────────────────────────────
        tb2 = QHBoxLayout()
        root.addLayout(tb2)

        tb2.addWidget(_muted_label("🔊  Audio:"))

        self.btn_audio = QPushButton("Enable Audio")
        self.btn_audio.setFixedWidth(130)
        self.btn_audio.setCheckable(True)
        self.btn_audio.setObjectName("btn_audio")
        if not AUDIO_AVAILABLE:
            self.btn_audio.setEnabled(False)
            self.btn_audio.setToolTip("pip install sounddevice")
        self.btn_audio.clicked.connect(self._on_audio_clicked)
        tb2.addWidget(self.btn_audio)

        tb2.addSpacing(16)
        tb2.addWidget(_muted_label("Volume:"))

        self.vol_slider = QSlider(Qt.Orientation.Horizontal)
        self.vol_slider.setRange(0, 100)
        self.vol_slider.setValue(50)
        self.vol_slider.setFixedWidth(150)
        self.vol_slider.valueChanged.connect(self._on_volume_changed)
        tb2.addWidget(self.vol_slider)

        self.vol_pct = QLabel("50 %")
        self.vol_pct.setObjectName("stat_val")
        self.vol_pct.setFixedWidth(42)
        tb2.addWidget(self.vol_pct)

        tb2.addSpacing(20)

        self.audio_status = QLabel("● Off")
        self.audio_status.setObjectName("audio_off")
        tb2.addWidget(self.audio_status)

        if not AUDIO_AVAILABLE:
            tb2.addWidget(_muted_label("  ⚠  pip install sounddevice"))

        tb2.addStretch()

        # ── Stats row ─────────────────────────────────────────────────────────
        stats = QHBoxLayout()
        root.addLayout(stats)

        def stat(key):
            stats.addWidget(_muted_label(key))
            v = QLabel("—")
            v.setObjectName("stat_val")
            stats.addWidget(v)
            stats.addSpacing(18)
            return v

        self.lbl_fs      = stat("Measured Fs:")
        self.lbl_packets = stat("Packets RX:")
        self.lbl_lost    = stat("Lost:")
        self.lbl_vmin    = stat("V min:")
        self.lbl_vmax    = stat("V max:")
        self.lbl_vrms    = stat("V rms:")
        stats.addStretch()

        line = QFrame()
        line.setFrameShape(QFrame.Shape.HLine)
        line.setObjectName("divider")
        root.addWidget(line)

        # ── Plots ─────────────────────────────────────────────────────────────
        plots = QVBoxLayout()
        root.addLayout(plots)

        pg.setConfigOptions(antialias=True, background=CLR_BG, foreground=CLR_FG)

        self.time_plot = pg.PlotWidget(title="Time Domain")
        self.time_plot.setLabel("left",   "Voltage", units="V")
        self.time_plot.setLabel("bottom", "Time",    units="ms")
        self.time_plot.showGrid(x=True, y=True, alpha=0.2)
        self.time_plot.setYRange(-0.05, self.vref + 0.05)
        self.time_curve = self.time_plot.plot(pen=pg.mkPen(CLR_CYAN, width=1))
        plots.addWidget(self.time_plot, stretch=2)

        self.fft_plot = pg.PlotWidget(title="FFT Spectrum")
        self.fft_plot.setLabel("left",   "Magnitude", units="dBFS")
        self.fft_plot.setLabel("bottom", "Frequency", units="kHz")
        self.fft_plot.showGrid(x=True, y=True, alpha=0.2)
        self.fft_plot.setXRange(0, self.fs / 2e3)
        self.fft_plot.setYRange(-100, 5)
        self.fft_curve = self.fft_plot.plot(
            pen=pg.mkPen(CLR_ORANGE, width=1),
            fillLevel=-100,
            brush=pg.mkBrush(QColor(CLR_ORANGE).darker(300))
        )
        self.peak_marker = pg.InfiniteLine(
            angle=90, movable=False,
            pen=pg.mkPen(CLR_RED, width=1, style=Qt.PenStyle.DashLine)
        )
        self.fft_plot.addItem(self.peak_marker)
        self.peak_label = pg.TextItem(color=CLR_RED, anchor=(0, 1))
        self.fft_plot.addItem(self.peak_label)
        plots.addWidget(self.fft_plot, stretch=2)

        self.hist_plot = pg.PlotWidget(title="ADC Code Histogram")
        self.hist_plot.setLabel("left",   "Count")
        self.hist_plot.setLabel("bottom", "ADC Code (0–1023)")
        self.hist_plot.showGrid(x=False, y=True, alpha=0.2)
        self.hist_bar = pg.BarGraphItem(x=[], height=[], width=14,
                                        brush=CLR_GREEN, pen=None)
        self.hist_plot.addItem(self.hist_bar)
        plots.addWidget(self.hist_plot, stretch=1)

        self.status = QStatusBar()
        self.setStatusBar(self.status)
        self.status.showMessage("Disconnected")

    def _apply_style(self):
        self.setStyleSheet(f"""
            QMainWindow, QWidget {{
                background-color: {CLR_BG};  color: {CLR_FG};
                font-family: 'Segoe UI', 'Inter', sans-serif;
                font-size: 13px;
            }}
            QLabel                {{ color: {CLR_FG}; }}
            QLabel#stat_key       {{ color: {CLR_MUTED}; font-size: 12px; }}
            QLabel#stat_val       {{ color: {CLR_CYAN};  font-size: 12px; font-weight: bold; }}
            QLabel#audio_on       {{ color: {CLR_GREEN}; font-size: 12px; font-weight: bold; }}
            QLabel#audio_off      {{ color: {CLR_MUTED}; font-size: 12px; }}
            QComboBox, QSpinBox, QDoubleSpinBox {{
                background: {CLR_SURFACE}; border: 1px solid {CLR_BORDER};
                border-radius: 4px; padding: 3px 6px; color: {CLR_FG};
            }}
            QPushButton {{
                background: {CLR_SURFACE}; border: 1px solid {CLR_BORDER};
                border-radius: 5px; padding: 5px 12px; color: {CLR_FG};
            }}
            QPushButton:hover    {{ background: {CLR_BORDER}; }}
            QPushButton:checked  {{ background: {CLR_ACCENT}; color: {CLR_BG}; font-weight: bold; }}
            QPushButton#btn_audio:checked {{ background: {CLR_GREEN}; color: {CLR_BG}; }}
            QFrame#divider       {{ background: {CLR_BORDER}; max-height: 1px; }}
            QStatusBar           {{ background: {CLR_SURFACE}; color: {CLR_MUTED}; font-size: 11px; }}
            QSlider::groove:horizontal {{
                height: 4px; background: {CLR_BORDER}; border-radius: 2px;
            }}
            QSlider::handle:horizontal {{
                background: {CLR_ACCENT}; border: none;
                width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;
            }}
            QSlider::sub-page:horizontal {{
                background: {CLR_ACCENT}; border-radius: 2px;
            }}
        """)

    # ── Helpers ───────────────────────────────────────────────────────────────
    def _populate_ports(self):
        try:
            from serial.tools.list_ports import comports
            for p in comports():
                if self.port_combo.findText(p.device) < 0:
                    self.port_combo.addItem(p.device)
        except Exception:
            pass

    def _set_audio_status(self, on: bool):
        if on:
            self.audio_status.setText("● Live")
            self.audio_status.setObjectName("audio_on")
        else:
            self.audio_status.setText("● Off")
            self.audio_status.setObjectName("audio_off")
        self.audio_status.style().unpolish(self.audio_status)
        self.audio_status.style().polish(self.audio_status)

    # ── Connection ────────────────────────────────────────────────────────────
    def _connect(self, port: str):
        baud = int(self.baud_combo.currentText())
        audio = self.audio if (AUDIO_AVAILABLE and self.btn_audio.isChecked()) else None
        self.reader = SerialReader(port, baud, self.queue, audio,
                                   esp32=self.esp32)
        self.reader.error.connect(self._on_serial_error)
        self.reader.start()
        self.t_start = time.time()
        self.total_samples = 0
        self.timer.start()
        self.btn_connect.setText("■  Disconnect")
        self.status.showMessage(f"Connected → {port}  @  {baud} baud")

    def _disconnect(self):
        self.timer.stop()
        if self.reader:
            self.reader.stop()
            self.reader = None
        if self.audio:
            self.audio.stop()
        self.btn_audio.setChecked(False)
        self._set_audio_status(False)
        self.btn_connect.setText("▶  Connect")
        self.status.showMessage("Disconnected")

    # ── Slots ─────────────────────────────────────────────────────────────────
    def _on_connect_clicked(self):
        if self.reader:
            self._disconnect()
        else:
            port = self.port_combo.currentText().strip()
            if port:
                self._connect(port)

    def _on_freeze_clicked(self, checked: bool):
        if checked:
            self.timer.stop()
            self.status.showMessage("Frozen — click Freeze again to resume")
        else:
            self.timer.start()
            self.status.showMessage("Running")

    def _on_vref_changed(self, v: float):
        self.vref = v
        self.time_plot.setYRange(-0.05, v + 0.05)

    def _on_serial_error(self, msg: str):
        self.status.showMessage(f"[ERROR] {msg}")
        self._disconnect()

    def _on_audio_clicked(self, checked: bool):
        if not AUDIO_AVAILABLE or not self.audio:
            return
        if checked:
            self.audio.set_volume(self.vol_slider.value() / 100)
            self.audio.start()
            if self.reader:
                self.reader.audio = self.audio
            self._set_audio_status(True)
            self.status.showMessage("Audio enabled — DC offset removed automatically")
        else:
            self.audio.stop()
            if self.reader:
                self.reader.audio = None
            self._set_audio_status(False)
            self.status.showMessage("Audio disabled")

    def _on_volume_changed(self, value: int):
        self.vol_pct.setText(f"{value} %")
        if self.audio:
            self.audio.set_volume(value / 100)

    # ── Plot refresh ──────────────────────────────────────────────────────────
    def _update_plots(self):
        new_samples = []
        try:
            while True:
                new_samples.append(self.queue.get_nowait())
        except queue.Empty:
            pass
        if not new_samples:
            return

        flat = np.concatenate(new_samples)
        self.total_samples += len(flat)
        n = len(flat)
        self.ring = np.roll(self.ring, -n)
        self.ring[-n:] = flat

        # Convert samples to volts
        if self.esp32:
            volts = self.ring * self.vref / 4095.0   # 12-bit codes → V
        elif self.millivolt:
            volts = self.ring / 1000.0               # mV → V (legacy)
        else:
            volts = self.ring * self.vref / 1023.0   # 10-bit AVR codes → V
        win_ms = self.window_spin.value()
        n_show = min(int(win_ms * 1e-3 * self.fs), self.ring_len)
        v_show = volts[-n_show:]
        t_axis = np.linspace(-win_ms, 0, n_show)
        self.time_curve.setData(t_axis, v_show)

        fft_data = volts[-self.fft_size:]
        win      = np.hanning(len(fft_data))
        spec     = np.abs(np.fft.rfft(fft_data * win)) * 2 / len(fft_data)
        spec_db  = 20 * np.log10(np.maximum(spec, 1e-9))
        freq_khz = np.fft.rfftfreq(len(fft_data), d=1.0 / self.fs) / 1e3
        self.fft_curve.setData(freq_khz, spec_db)

        peak_idx = int(np.argmax(spec_db[1:])) + 1
        peak_f   = freq_khz[peak_idx]
        peak_db  = spec_db[peak_idx]
        self.peak_marker.setValue(peak_f)
        self.peak_label.setText(f" {peak_f:.2f} kHz\n {peak_db:.1f} dBFS")
        self.peak_label.setPos(peak_f, peak_db)

        if self.esp32:
            self.hist_plot.setLabel("bottom", "ADC Code (0–4095, 12-bit)")
            counts, edges = np.histogram(self.ring[-n_show:], bins=64, range=(0, 4095))
        elif self.millivolt:
            self.hist_plot.setLabel("bottom", "Millivolts (0–3300 mV)")
            counts, edges = np.histogram(self.ring[-n_show:], bins=64, range=(0, 3300))
        else:
            self.hist_plot.setLabel("bottom", "ADC Code (0–1023, 10-bit)")
            counts, edges = np.histogram(self.ring[-n_show:], bins=64, range=(0, 1023))
        centres = (edges[:-1] + edges[1:]) / 2
        self.hist_bar.setOpts(x=centres, height=counts,
                              width=(edges[1] - edges[0]) * 0.9)

        elapsed = time.time() - self.t_start
        mfs = self.total_samples / elapsed if elapsed > 0 else 0
        self.lbl_fs.setText(f"{mfs:.0f} Hz")
        if self.reader:
            self.lbl_packets.setText(str(self.reader.packets_rx))
            lost = self.reader.packets_lost
            self.lbl_lost.setText(str(lost))
            self.lbl_lost.setStyleSheet(
                f"color: {CLR_RED};" if lost > 0 else f"color: {CLR_GREEN};")
        self.lbl_vmin.setText(f"{v_show.min():.3f} V")
        self.lbl_vmax.setText(f"{v_show.max():.3f} V")
        self.lbl_vrms.setText(f"{np.sqrt(np.mean(v_show**2)):.3f} V")

    def closeEvent(self, event):
        self._disconnect()
        event.accept()


# ── Utility ────────────────────────────────────────────────────────────────────
def _muted_label(text: str) -> QLabel:
    lbl = QLabel(text)
    lbl.setObjectName("stat_key")
    return lbl


# ══════════════════════════════════════════════════════════════════════════════
#  Entry point
# ══════════════════════════════════════════════════════════════════════════════
def main():
    ap = argparse.ArgumentParser(
        description="PyQt6 live monitor + audio for Arduino ADC 44 kHz")
    ap.add_argument("--port",  default="")
    ap.add_argument("--baud",  type=int,   default=921_600)
    ap.add_argument("--fs",    type=float, default=22_039)
    ap.add_argument("--vref",  type=float, default=5.0)
    ap.add_argument("--esp32", action="store_true",
                    help="ESP32 mode: samples are 12-bit ADC codes (0-4095), "
                         "Vref is 3.3 V. Replaces 10-bit AVR conversion.")
    args = ap.parse_args()

    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = MainWindow(args)
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
