# flutter_application_2
# Aceptame en el repo

A new Flutter project.

## Getting Started

This project is a starting point for a Flutter application.

A few resources to get you started if this is your first Flutter project:

- [Lab: Write your first Flutter app](https://docs.flutter.dev/get-started/codelab)
- [Cookbook: Useful Flutter samples](https://docs.flutter.dev/cookbook)

For help getting started with Flutter development, view the
[online documentation](https://docs.flutter.dev/), which offers tutorials,
samples, guidance on mobile development, and a full API reference.

sudo apt install -y \
    libqt6gui6 \
    libqt6widgets6 \
    libqt6core6 \
    libgl1 \
    libatlas-base-dev \
    libxcb-xinerama0

python3 -m venv audio_env
source audio_env/bin/activate

pip install PyQt6 pyqtgraph numpy

si Could not load the Qt platform plugin "xcb"

sudo apt install libxcb-cursor0 -y

python3 -c "from PyQt6.QtWidgets import QApplication; print('Qt OK')"

import os
os.environ["QT_OPENGL"] = "software"
