from PyQt6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel, QSlider, QFrame
from PyQt6.QtCore import Qt, pyqtSignal

class EffectWidget(QWidget):
    param_changed = pyqtSignal(str, str, float)
    delete_requested = pyqtSignal(str)

    PARAM_RANGES = {
        "GAIN":       (0, 1, ""),
        "TONE":       (0, 1, ""),
        "OUTPUT":     (0, 1, ""),
        "TIME":       (1, 1000, "ms"),
        "FEEDBACK":   (0, 0.8, ""),
        "MIX":        (0, 1, ""),
        "FREQ":       (300, 4000, "Hz"),
        "Q":          (0.1, 10, ""),
        "LEVEL":      (0, 1, ""),
        "RATE":       (0.1, 3, "Hz"),
        "DEPTH":      (0, 1, ""),
        "SEMITONES":  (-12, 12, "st"),
        "SEMITONES_B":(-12, 12, "st"),
        "MIX_A":      (0, 1, ""),
        "MIX_B":      (0, 1, ""),
    }

    def __init__(self, effect_data):
        super().__init__()

        self.effect_data = effect_data
        self.effect_id = effect_data["id"]
        self.expanded = False

        self.main_layout = QVBoxLayout()

        # HEADER con nombre + "x"
        self.header_widget = QWidget()
        self.header_layout = QHBoxLayout()
        self.header_widget.setLayout(self.header_layout)
        self.header_layout.setContentsMargins(0, 0, 0, 0)
        self.header_layout.setSpacing(5)
        self.header_widget.setStyleSheet("""
            background-color: #e0e0e0;
            border-radius: 6px;
            padding: 6px;
        """)
        self.header_widget.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setLayout(self.main_layout)

        
        #Linea divisoria
        line = QFrame()
        line.setFrameShape(QFrame.Shape.HLine)
        line.setFrameShadow(QFrame.Shadow.Sunken)
        self.main_layout.addWidget(line)  

        # Label con el nombre del efecto
        self.header_label = QLabel(effect_data["type"])
        self.header_label.setStyleSheet("""
            font-family: Arial;
            font-size: 11pt;
        """)
        self.header_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.header_label.mousePressEvent = self.toggle_expand  

        self.header_layout.addWidget(self.header_label, 1)

        # Botón "x" para eliminar
        self.delete_btn = QPushButton("✕")
        self.delete_btn.setFixedSize(20, 20)
        self.delete_btn.setStyleSheet("border: none; color: red; font-weight: bold;")
        self.delete_btn.clicked.connect(self.delete_self)
        self.header_layout.addStretch()
        self.header_layout.addWidget(self.delete_btn)

        self.main_layout.addWidget(self.header_widget)

        # Parametros
        self.params_Widget = QWidget()
        self.params_Layout = QVBoxLayout()
        self.params_Widget.setLayout(self.params_Layout)
        self.sliders = {}

        for param, value in effect_data["params"].items():
            min_val, max_val, unit = self.PARAM_RANGES.get(param, (0, 1, ""))
            label = QLabel(f"{param}: {value} {unit}")
            
            slider = QSlider(Qt.Orientation.Horizontal)
            slider.setMinimum(0)
            slider.setMaximum(100)
            slider_value = int((value - min_val) / (max_val - min_val) * 100)
            slider.setValue(slider_value)

            slider.valueChanged.connect(
                lambda val, p=param, l=label: self.update_label_only(p, val, l)
            )

            slider.sliderReleased.connect(
                lambda p=param, s=slider, l=label: self.update_param(p, s.value(), l)
            )

            self.params_Layout.addWidget(label)
            self.params_Layout.addWidget(slider)
            self.sliders[param] = slider

        self.main_layout.addWidget(self.params_Widget)
        self.params_Widget.setVisible(False)

    def toggle_expand(self, event=None):
        self.expanded = not self.expanded
        self.params_Widget.setVisible(self.expanded)
        arrow = "▲" if self.expanded else "▼"
        self.header_label.setText(self.effect_data["type"] + " " + arrow)
        if hasattr(self, "list_item"):
            self.list_item.setSizeHint(self.sizeHint())

    def delete_self(self):
        self.delete_requested.emit(self.effect_id)

    def update_param(self, param, value, label):
        min_val, max_val, unit = self.PARAM_RANGES.get(param, (0, 1, ""))
        real_value = min_val + (value / 100) * (max_val - min_val)
        label.setText(f"{param}: {round(real_value,2)} {unit}")
        self.param_changed.emit(self.effect_id, param, real_value)
    
    def update_label_only(self, param, value, label):
        min_val, max_val, unit = self.PARAM_RANGES.get(param, (0, 1, ""))
        real_value = min_val + (value / 100) * (max_val - min_val)
        label.setText(f"{param}: {round(real_value, 2)} {unit}")
