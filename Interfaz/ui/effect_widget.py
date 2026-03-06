from PyQt6.QtWidgets import QWidget, QVBoxLayout, QPushButton, QLabel, QSlider, QListWidgetItem 
from PyQt6.QtCore import Qt, pyqtSignal

class EffectWidget(QWidget):
    param_changed = pyqtSignal(str, str, float)
    delete_requested = pyqtSignal(str)

    def __init__(self, effect_data):
        super().__init__()

        self.list_item = None
        
        self.effect_data = effect_data
        self.effect_id = effect_data["id"]
        self.expanded = False

        self.main_layout = QVBoxLayout()
        self.setLayout(self.main_layout)
        
        #Header
        self.header_button = QPushButton(effect_data["type"] + " ▼")
        self.header_button.setCheckable(True)
        self.header_button.clicked.connect(self.toggle_expand)
        self.main_layout.addWidget(self.header_button)

        #Boton para borrar efectos
        self.delete_button = QPushButton("Delete")
        self.delete_button.clicked.connect(self.delete_self)
        self.main_layout.addWidget(self.delete_button)

        #Parametros
        self.params_Widget = QWidget()
        self.params_Layout = QVBoxLayout()
        self.params_Widget.setLayout(self.params_Layout)
        self.sliders = {}


        #Sliders
        for param, value in effect_data["params"].items():
            min_val, max_val, unit = self.PARAM_RANGES.get(param, (0, 1, ""))
            label = QLabel(f"{param}: {value} {unit}")
            
            slider = QSlider(Qt.Orientation.Horizontal)
            slider.setMinimum(0)
            slider.setMaximum(100)

            slider_value = int((value - min_val) / (max_val - min_val) * 100)
            slider.setValue(slider_value)

            slider.valueChanged.connect(
                lambda val, p=param, l=label: self.update_param(p, val, l)
                
            )
            self.params_Layout.addWidget(label)
            self.params_Layout.addWidget(slider)
            self.sliders[param] = slider
        
        self.main_layout.addWidget(self.params_Widget)
        self.params_Widget.setVisible(False)

    PARAM_RANGES= {
        "GAIN": (0, 1, ""),
        "TONE": (0, 1, ""),
        "OUTPUT": (0, 1, ""),

        "TIME": (1, 1000, "ms"),     # ms
        "FEEDBACK": (0, 0.95, ""),
        "MIX": (0, 1, ""),

        "FREQ": (300, 2000, "Hz"),   # Hz
        "Q": (0.1, 10, ""),
        "LEVEL": (0, 1, ""),

        "RATE": (0.1, 10, "Hz"),     # Hz
        "DEPTH": (0, 1, ""),

        "SEMITONES": (-12, 12, "st")
    }
    
    def delete_self(self):
        self.delete_requested.emit(self.effect_id)

    def toggle_expand(self):
        self.expanded = not self.expanded
        self.params_Widget.setVisible(self.expanded)

        arrow = "▲" if self.expanded else "▼"
        self.header_button.setText(self.effect_data["type"] + " " + arrow)

        if hasattr(self, "list_item"):
            self.list_item.setSizeHint(self.sizeHint())

    def update_param(self, param, value, label):

        min_val, max_val, unit = self.PARAM_RANGES.get(param, (0, 1, ""))

        real_value = min_val + (value / 100) * (max_val - min_val)

        label.setText(f"{param}: {round(real_value, 2)} {unit}")

        self.param_changed.emit(self.effect_id, param, real_value)

