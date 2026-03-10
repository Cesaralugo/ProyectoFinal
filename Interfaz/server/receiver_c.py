from PyQt6.QtCore import QObject, pyqtSignal
import socket
import struct
import threading
import os
import time

BATCH_SIZE = 128   # debe coincidir con SERIAL_PACKET_SAMPLES en C

class SocketReceiver(QObject):
    # Ahora emite arrays numpy-compatibles (lista de floats) en vez de un float
    # para evitar 22 000 señales Qt por segundo
    batch_received = pyqtSignal(list, list)   # (pre_batch, post_batch)

    # Señales legacy — se siguen emitiendo para no romper código existente,
    # pero solo con el PRIMER sample del lote (para quien las use aún)
    pre_received  = pyqtSignal(float)
    post_received = pyqtSignal(float)

    def __init__(self, socket_path="/tmp/audio_socket"):
        super().__init__()
        self.socket_path = socket_path
        self.running = False
        self.client = None

    def start(self):
        self.running = True
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        self.client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client = self.client

        while self.running:
            try:
                if os.path.exists(self.socket_path):
                    client.connect(self.socket_path)
                    break
            except ConnectionRefusedError:
                pass
            time.sleep(0.05)

        # Cada lote = BATCH_SIZE pares de float32 = BATCH_SIZE * 2 * 4 bytes
        BATCH_BYTES = BATCH_SIZE * 2 * 4

        def recv_exact(sock, n):
            data = b''
            while len(data) < n:
                chunk = sock.recv(n - len(data))
                if not chunk:
                    return None
                data += chunk
            return data

        try:
            while self.running:
                raw = recv_exact(client, BATCH_BYTES)
                if raw is None:
                    time.sleep(0.05)
                    continue

                # Desempaquetar BATCH_SIZE * 2 floats intercalados [pre0,post0,pre1,post1,...]
                floats = struct.unpack(f'<{BATCH_SIZE * 2}f', raw)
                pre_batch  = floats[0::2]   # índices pares
                post_batch = floats[1::2]   # índices impares

                self.batch_received.emit(list(pre_batch), list(post_batch))

                # Legacy: emitir solo el primer sample para compatibilidad
                self.pre_received.emit(pre_batch[0])
                self.post_received.emit(post_batch[0])

        finally:
            client.close()

    def send_json(self, json_data):
        if self.client is None:
            return
        try:
            msg = (json_data + "\n").encode('utf-8')
            self.client.sendall(msg)
        except BrokenPipeError:
            print("Socket cerrado")

    def stop(self):
        self.running = False