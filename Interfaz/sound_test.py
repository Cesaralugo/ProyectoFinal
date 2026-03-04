import sounddevice as sd
import numpy as np
from collections import deque
from server.receiver_c import SocketReceiver  # tu receptor por socket
from PyQt6.QtCore import QTimer

SAMPLE_RATE = 44100  # igual que en tu C

# Buffer circular para reproducir
audio_buffer = deque(maxlen=8192)

# Función para enviar la señal post al buffer
def update_buffer(value):
    audio_buffer.append(value)

# Configurar receptor
receiver = SocketReceiver()
receiver.post_received.connect(update_buffer)
receiver.start()

# Función callback de sonido
def audio_callback(outdata, frames, time, status):
    if status:
        print(status)
    # Sacamos frames del buffer
    data = np.zeros(frames, dtype=np.float32)
    for i in range(frames):
        if audio_buffer:
            data[i] = audio_buffer.popleft()
    outdata[:] = data.reshape(-1, 1)  # mono

# Abrir stream de audio
stream = sd.OutputStream(
    samplerate=SAMPLE_RATE,
    channels=1,
    dtype='float32',
    callback=audio_callback,
    blocksize=256  # cuanto se procesa por bloque
)

with stream:
    import time
    while True:
        time.sleep(0.1)