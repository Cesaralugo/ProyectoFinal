import sys
import numpy as np
import pyqtgraph as pg
import json
import os

from ui.effect_widget import EffectWidget
from core.preset_model import PresetModel
from server.receiver_app import TcpServer
from server.receiver_c import SocketReceiver

from collections import deque
from PyQt6.QtWidgets import QApplication, QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QSlider, QLabel, QComboBox, QListWidget, QListWidgetItem 
from PyQt6.QtCore import QTimer, Qt
PRESETS_FILE = "presets.json"

class MainWindow(QWidget):
    SAMPLE_RATE = 44100  
    def __init__(self):
        super().__init__()  
    
        self.setWindowTitle("Audio Interface")

        #Conexión con C
        self.receiver = SocketReceiver()
        self.receiver.batch_received.connect(self.update_buffers_batch)
        self.receiver.start()

        self.pre_buffer = deque(maxlen=8192)
        self.signal_buffer = deque(maxlen=8192)

        self.t = 0
        
        #Definimos el tipo de layer
        self.main_layout = QHBoxLayout()
        self.setLayout(self.main_layout)

        self.left_layout = QVBoxLayout()

        #Titulo
        self.title_label = QLabel("MultiFX Processor")
        self.left_layout.addWidget(self.title_label)
        
        #Presets Dropdown
        self.presets_data = self._load_presets_file()
        self.current_preset_key = "Preset 1"

        self.preset_dropdown = QComboBox()
        self.preset_dropdown.addItems(list(self.presets_data.keys()))
        self.preset_dropdown.currentTextChanged.connect(self.on_preset_changed)
        self.left_layout.addWidget(self.preset_dropdown)

        #Effects Dropdown
        self.available_effects = [
            "Overdrive",
            "Delay",
            "Wah",
            "Flanger",
            "Chorus",
            "Phaser",
            "PitchShifter"
        ]

        #OnClick add effect
        self.add_effect_box = QComboBox()
        self.add_effect_box.addItems(self.available_effects)
        self.add_effect_box.activated.connect(self.add_effect)  
        self.left_layout.addWidget(self.add_effect_box)

        #Lista de efectos
        self.effects_list = QListWidget()
        self.effects_list.setStyleSheet("""
        QListWidget::item:selected {
            background: #dcdcdc;
        }

        QListWidget::item:hover {
            background: #e6e6e6;
        }
        """)
        self.effects_list.setDragDropMode(QListWidget.DragDropMode.InternalMove)
        self.effects_list.model().rowsMoved.connect(self.update_effect_order)
        
        self.model = PresetModel("Preset1")
        first = self.presets_data[self.current_preset_key]
        self.model.set_effects(first.get("effects", []))


        #Cargar efectos
        self.load_effects()
        self.left_layout.addWidget(self.effects_list)
        
        self.right_layout = QVBoxLayout()

        self.main_layout.addLayout(self.left_layout, 1)
        self.main_layout.addLayout(self.right_layout, 2)
        
        # Estado inicial
        self.show_fft = False

        # Color de los ejes
        label_style = {'color': 'white', 'font-size': '11pt'}
        title_style = {'color': 'white', 'size': '13pt'}

        #Plot de las señales

        # Pre
        self.plot_pre = pg.PlotWidget()
        self.plot_pre.setTitle("Pre Effect", **title_style)
        self.plot_pre.setLabel("left", "Amplitude", **label_style)
        self.plot_pre.setLabel("bottom", "Time", **label_style)
        self.plot_pre.getAxis("left").setTextPen('white')
        self.plot_pre.getAxis("bottom").setTextPen('white')
        self.curve_pre = self.plot_pre.plot(
            pen=pg.mkPen(color=(0, 180, 255), width=1.5),
        )
        self.right_layout.addWidget(self.plot_pre)

        # Post
        self.plot_post = pg.PlotWidget()
        self.plot_post.setTitle("Post Effect", **title_style)
        self.plot_post.setLabel("left", "Amplitude", **label_style)
        self.plot_post.setLabel("bottom", "Time", **label_style)
        self.plot_post.getAxis("left").setTextPen('white')
        self.plot_post.getAxis("bottom").setTextPen('white')
        self.curve_post = self.plot_post.plot(
            pen=pg.mkPen(color=(0, 180, 255), width=1.5),
        )
        self.right_layout.addWidget(self.plot_post)

        # Botón toggle
        self.toggle_fft_btn = QPushButton("Show FFT")
        self.toggle_fft_btn.setCheckable(True)
        self.toggle_fft_btn.clicked.connect(self.toggle_fft)
        self.right_layout.addWidget(self.toggle_fft_btn)
        
        self.timer = QTimer()
        self.timer.timeout.connect(self.sim_signal)
        self.timer.start(80) #Elegir velocidad en la que se generan los puntos

        self.server = TcpServer()
        self.server.json_received.connect(self.handle_remote_json)
        self.server.start()

    def update_buffers_batch(self, pre_batch, post_batch):
        self.pre_buffer.extend(pre_batch)
        self.signal_buffer.extend(post_batch)

    def update_effect_order(self, *args):
        new_order = []

        for i in range(self.effects_list.count()):
            item = self.effects_list.item(i)
            widget = self.effects_list.itemWidget(item)
            new_order.append(widget.effect_data)

        self.model.update_order(new_order)
        self._save_current_preset()
        json_data = self.model.to_json()
        self.receiver.send_json(json_data)
        print(self.model.to_json())

    def handle_remote_json(self,data):
        print("Actualizando desde el celular")

        self.model.load_from_json(data)

        self.load_effects()

        print("Nuevo estado: ")
        print(self.model.to_json())
    
    def _load_presets_file(self):
        if not os.path.exists(PRESETS_FILE):
            default = {
                "Preset 1": {"name": "Preset1", "effects": []},
                "Preset 2": {"name": "Preset2", "effects": []},
                "Preset 3": {"name": "Preset3", "effects": []},
            }
            with open(PRESETS_FILE, "w") as f:
                json.dump(default, f, indent=2)
        
        with open(PRESETS_FILE, "r") as f:
            return json.load(f)

    def _save_presets_file(self, presets_data):
        with open(PRESETS_FILE, "w") as f:
            json.dump(presets_data, f, indent=2)

    def on_preset_changed(self, preset_key):
        # Guardar el preset actual antes de cambiar
        self._save_current_preset()

        # Cargar el nuevo preset
        self.current_preset_key = preset_key
        preset = self.presets_data[preset_key]
        self.model = PresetModel(preset["name"])
        self.model.set_effects(preset.get("effects", []))

        self.signal_buffer.clear()
        self.pre_buffer.clear()
        self.load_effects()

        json_data = self.model.to_json()
        self.receiver.send_json(json_data)

    def _save_current_preset(self):
        import json as _json
        # Leer el JSON del modelo actual y guardarlo en el dict en memoria
        raw = self.model.to_json()
        parsed = _json.loads(raw)
        self.presets_data[self.current_preset_key] = {
            "name": parsed["name"],
            "effects": parsed["effects"]
        }
        self._save_presets_file(self.presets_data)

    #Añadir efectos logic
    def add_effect(self):
        if len(self.model.effects) >= 4:
            print("Max 4 effects per parameter")
            return
        effect_type = self.add_effect_box.currentText()

        already_exists = any(e["type"] == effect_type for e in self.model.effects)
        if already_exists:
            print(f"{effect_type} ya esta en la cadena")
            return

        new_id = f"fx_{len(self.model.effects)+1}"

        effect = {
            "id": new_id,
            "type": effect_type,
            "enabled": True,
            "params": self.default_params(effect_type)
        }

        self.model.effects.append(effect)
        self.load_effects()
        self._save_current_preset()

        json_data = self.model.to_json()
        self.receiver.send_json(json_data)

    def remove_effect(self, effect_id):
        print("Removing effect:", effect_id)

        self.model.effects = [
            e for e in self.model.effects if e["id"] != effect_id
        ]

        self.signal_buffer.clear()
        self.pre_buffer.clear()  

        self.load_effects()
        self._save_current_preset()

        json_data = self.model.to_json()
        self.receiver.send_json(json_data)  
    
    #Cargar efectos
    def load_effects(self):
        self.effects_list.clear()
        
        for effect in self.model.effects:
            item = QListWidgetItem()
            widget = EffectWidget(effect)
            widget.list_item = item
            
            item.setSizeHint(widget.sizeHint())

            widget.param_changed.connect(self.handle_param_change)
            widget.delete_requested.connect(self.remove_effect)

            self.effects_list.addItem(item)
            self.effects_list.setItemWidget(item, widget)

    def default_params(self, effect_type):

        defaults = {
            "Overdrive": {"GAIN":0.5,"TONE":0.5,"OUTPUT":0.5},
            "Delay": {"TIME":0.5,"FEEDBACK":0.3,"MIX":0.2},
            "Wah": {"FREQ":0.5,"Q":0.3,"LEVEL":0.2},
            "Flanger": {"RATE":0.5,"DEPTH":0.3,"FEEDBACK":0.2,"MIX":0.5},
            "Chorus": {"RATE":0.5,"DEPTH":0.5,"MIX":0.5},
            "Phaser": {"RATE":0.5,"DEPTH":0.7,"FEEDBACK":0.3,"MIX":0.5},
            "PitchShifter": {"SEMITONES":0.0,"MIX":0.5}
        }

        return defaults[effect_type]
    
    #Generación del Json
    def generate_json(self):
        print("JSON ready for C++: ")
        print(self.model.to_json())

    def update_buffer(self, value):
        self.signal_buffer.append(value)

    def update_pre_buffer(self, value):
        self.pre_buffer.append(value)
        #if len(self.signal_buffer) % 200 == 0:
            #print("post buffer:", len(self.signal_buffer))
    
    def update_buffers_batch(self, pre_batch, post_batch):
        VREF = 3.3
        pre_volts  = [(x + 1.0) * (VREF / 2.0) for x in pre_batch]
        post_volts = [(x + 1.0) * (VREF / 2.0) for x in post_batch]
        self.pre_buffer.extend(pre_volts)
        self.signal_buffer.extend(post_volts)

    def _compute_fft(self, buffer, accum_key):
        N_FFT = 4096
        y = np.array(buffer, dtype=float)
        
        if len(y) < N_FFT:
            y = np.pad(y, (0, N_FFT - len(y)), 'constant')
        else:
            y = y[-N_FFT:]  # usar los samples más recientes

        y -= np.mean(y)  
        window = np.blackman(N_FFT)
        Y = np.abs(np.fft.rfft(y * window)) * 2.0 / np.sum(window)
        Y_db = 20 * np.log10(Y + 1e-12)

        prev = getattr(self, accum_key, None)
        if prev is None or prev.shape != Y_db.shape:
            setattr(self, accum_key, Y_db)
        else:
            smoothed = 0.7 * prev + 0.3 * Y_db  # α=0.3 
            setattr(self, accum_key, smoothed)

        freqs = np.fft.rfftfreq(N_FFT, d=1.0 / self.SAMPLE_RATE)
        return freqs, getattr(self, accum_key)

    def sim_signal(self):
        pre_data  = np.array(self.pre_buffer)
        post_src  = self.pre_buffer if len(self.model.effects) == 0 else self.signal_buffer
        post_data = np.array(post_src)

        if len(pre_data) > 0:
            pre_data = pre_data - np.mean(pre_data)
        if len(post_data) > 0:
            post_data = post_data - np.mean(post_data)

        x_pre  = np.arange(len(pre_data))
        x_post = np.arange(len(post_data))

        if not self.show_fft:
            self.plot_pre.setLabel("bottom", "Time")
            self.plot_pre.setLabel("left", "Amplitude")
            self.plot_pre.enableAutoRange()
            self.curve_pre.setData(x_pre, pre_data)
        else:
            self.plot_pre.setLabel("bottom", "Frequency (Hz)")
            self.plot_pre.setLabel("left", "Magnitude (dBFS)")
            freqs, Y_db = self._compute_fft(self.pre_buffer, '_fft_pre')
            mask = freqs <= 20000
            self.plot_pre.setXRange(0, 20000)
            # Dinámico: pico real como techo, -90 como piso
            peak = float(np.max(Y_db[mask]))
            self.plot_pre.setYRange(-150, peak + 5)
            self.curve_pre.setData(freqs[mask], Y_db[mask])

        if not self.show_fft:
            self.plot_post.setLabel("bottom", "Time")
            self.plot_post.setLabel("left", "Amplitude")
            self.plot_post.enableAutoRange()
            self.curve_post.setData(x_post, post_data)
        else:
            self.plot_post.setLabel("bottom", "Frequency (Hz)")
            self.plot_post.setLabel("left", "Magnitude (dBFS)")
            freqs, Y_db = self._compute_fft(post_src, '_fft_post')
            mask = freqs <= 20000
            self.plot_post.setXRange(0, 20000)
            peak = float(np.max(Y_db[mask]))
            self.plot_post.setYRange(-150, peak + 5)
            self.curve_post.setData(freqs[mask], Y_db[mask])

    def handle_param_change(self, effect_id, param, value):
        print("MainWindow updating model")

        self.model.update_param(effect_id, param, value)

        json_data = self.model.to_json()

        print("JSON ready for C++:")
        print(json_data)

        self.receiver.send_json(json_data)
        
    def toggle_fft(self):
        self.show_fft = self.toggle_fft_btn.isChecked()
        if self.show_fft:
            self.toggle_fft_btn.setText("Show Time")
            # Activar fill para FFT
            self.curve_pre.setFillLevel(-150)
            self.curve_post.setFillLevel(-150)
            self.curve_pre.setBrush(pg.mkBrush(0, 140, 255, 60))
            self.curve_post.setBrush(pg.mkBrush(0, 140, 255, 60))
        else:
            self.toggle_fft_btn.setText("Show FFT")
            # Desactivar fill para tiempo
            self.curve_pre.setFillLevel(None)
            self.curve_post.setFillLevel(None)
            self.curve_pre.setBrush(None)
            self.curve_post.setBrush(None)
