import sys
import numpy as np
import pyqtgraph as pg
import json

from ui.effect_widget import EffectWidget
from core.preset_model import PresetModel
from server.receiver_app import TcpServer
from server.receiver_c import SocketReceiver

from collections import deque
from PyQt6.QtWidgets import QApplication, QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QSlider, QLabel, QComboBox, QListWidget, QListWidgetItem 
from PyQt6.QtCore import QTimer, Qt

class MainWindow(QWidget):
    SAMPLE_RATE = 44100  

    def __init__(self):
        super().__init__()  

        self.setWindowTitle("Audio Interface")
        
        #Conexión con C
        self.receiver = SocketReceiver()
        self.receiver.pre_received.connect(self.update_pre_buffer)
        self.receiver.post_received.connect(self.update_buffer)
        self.receiver.start()

        self.pre_buffer = deque([0]*500, maxlen=1000)
        self.signal_buffer = deque([0]*500, maxlen=1000)



        self.t = 0
        
        #Definimos el tipo de layer
        self.main_layout = QHBoxLayout()
        self.setLayout(self.main_layout)

        self.left_layout = QVBoxLayout()

        #Titulo
        self.title_label = QLabel("MultiFX Processor")
        self.left_layout.addWidget(self.title_label)
        
        #Dropdown
        self.preset_dropdown = QComboBox()
        self.preset_dropdown.addItems(["Preset 1", "Preset 2", "Preset 3"])
        self.left_layout.addWidget(self.preset_dropdown)
        
        #Lista de efectos
        self.effects_list = QListWidget()
        self.effects_list.setDragDropMode(QListWidget.DragDropMode.InternalMove)
        self.effects_list.model().rowsMoved.connect(self.update_effect_order)
        self.model = PresetModel("Preset1")

        initial_effects = [
            {"id": "fx_1", "type": "Distortion", "enabled": True,
            "params": {"GAIN": 0.5, "TONE": 0.5, "OUTPUT": 0.5}},

            {"id": "fx_2", "type": "Delay", "enabled": True,
            "params": {"TIME": 0.5, "FEEDBACK": 0.3, "MIX": 0.2}},

            {"id": "fx_3", "type": "Wah", "enabled": True,
            "params": {"FREQ": 0.5, "Q": 0.3, "LEVEL": 0.2}},

            {"id": "fx_4", "type": "Flanger", "enabled": True,
            "params": {"RATE": 0.5, "DEPTH": 0.3, "FEEDBACK": 0.2, "MIX": 0.5}},
        ]

        self.model.set_effects(initial_effects)

        #Cargar efectos
        self.load_effects()
        self.left_layout.addWidget(self.effects_list)
        
        self.right_layout = QVBoxLayout()

        self.main_layout.addLayout(self.left_layout, 1)
        self.main_layout.addLayout(self.right_layout, 2)
        
        # Botón toggle
        self.toggle_fft_btn = QPushButton("Show FFT")
        self.toggle_fft_btn.setCheckable(True)
        self.toggle_fft_btn.clicked.connect(self.toggle_fft)
        self.right_layout.addWidget(self.toggle_fft_btn)
        
        # Estado inicial
        self.show_fft = False

        #Plot de las señales
        self.plot_pre = pg.PlotWidget(title="Pre Effect Signal")
        self.plot_pre.setTitle("Pre Effect", size="12pt")
        self.plot_pre.setLabel("left", "Amplitude")
        self.plot_pre.setLabel("bottom", "Time")
        self.curve_pre = self.plot_pre.plot()
        self.right_layout.addWidget(self.plot_pre)

        self.plot_post = pg.PlotWidget(title="Post Effect Signal")
        self.plot_post.setTitle("Post Effect", size="12pt")
        self.plot_post.setLabel("left", "Amplitude")
        self.plot_post.setLabel("bottom", "Time")
        self.curve_post = self.plot_post.plot()
        self.right_layout.addWidget(self.plot_post)

        self.timer = QTimer()
        self.timer.timeout.connect(self.sim_signal)
        self.timer.start(150) #Elegir velocidad en la que se generan los puntos

        self.server = TcpServer()
        self.server.json_received.connect(self.handle_remote_json)
        self.server.start()

    def update_effect_order(self, *args):
        new_order = []

        for i in range(self.effects_list.count()):
            item = self.effects_list.item(i)
            widget = self.effects_list.itemWidget(item)
            new_order.append(widget.effect_data)

        self.model.update_order(new_order)
        print(self.model.to_json())

    def handle_remote_json(self,data):
        print("Actualizando desde el celular")

        self.model.load_from_json(data)

        self.load_effects()

        print("Nuevo estado: ")
        print(self.model.to_json())

    
    #Cargar efectos
    def load_effects(self):
        self.effects_list.clear()
        
        for effect in self.model.effects:
            item = QListWidgetItem()
            widget = EffectWidget(effect)
            widget.list_item = item
            
            item.setSizeHint(widget.sizeHint())

            widget.param_changed.connect(self.handle_param_change)
            self.effects_list.addItem(item)
            self.effects_list.setItemWidget(item, widget)
    
    #Generación del Json
    def generate_json(self):
        print("JSON ready for C++: ")
        print(self.model.to_json())


    def update_buffer(self, value):
        self.signal_buffer.append(value)

    def update_pre_buffer(self, value):
        self.pre_buffer.append(value)

    # Recepción de señales
    def sim_signal(self):
        # Eje X separado
        x_pre = np.arange(len(self.pre_buffer))
        x_post = np.arange(len(self.signal_buffer))

        if not self.show_fft:
            # Señal en el tiempo
            self.plot_post.setLabel("bottom", "Time")
            self.plot_post.setLabel("left", "Amplitude")
            self.curve_pre.setData(x_pre, list(self.pre_buffer))
            self.curve_post.setData(x_post, list(self.signal_buffer))
        else:
            # FFT de la señal post-efecto
            self.plot_post.setLabel("bottom", "Frequency (Hz)")
            self.plot_post.setLabel("left", "Magnitude")

            y = np.array(self.signal_buffer, dtype=float)

            # Zero-padding
            N_fft = 4096
            if len(y) < N_fft:
                y = np.pad(y, (0, N_fft - len(y)), 'constant')

            window = np.hanning(len(y))
            y_win = y * window
            Y = np.fft.rfft(y_win)
            Y_mag_db = 20 * np.log10(np.abs(Y) / len(Y) + 1e-12)
            freqs = np.fft.rfftfreq(N_fft, d=1.0/self.SAMPLE_RATE)

            self.curve_post.setData(freqs, Y_mag_db)
            self.curve_pre.setData(x_pre, list(self.pre_buffer))

    def handle_param_change(self, effect_id, param, value):
        print("MainWindow updating model")

        self.model.update_param(effect_id, param, value)

        print("JSON ready for C++:")
        print(self.model.to_json())
        
    def toggle_fft(self):
        self.show_fft = self.toggle_fft_btn.isChecked()
        if self.show_fft:
            self.toggle_fft_btn.setText("Show Time")
        else:
            self.toggle_fft_btn.setText("Show FFT")
