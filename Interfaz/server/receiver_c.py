from PyQt6.QtCore import QObject, pyqtSignal
import socket
import struct
import threading
import os
import time

class SocketReceiver(QObject):
    pre_received = pyqtSignal(float)
    post_received = pyqtSignal(float)

    def __init__(self, socket_path="/tmp/audio_socket"):
        super().__init__()
        self.socket_path = socket_path
        self.running = False

    def start(self):
        self.running = True
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

        # Espera a que el socket exista y el servidor esté listo
        while self.running:
            try:
                if os.path.exists(self.socket_path):
                    client.connect(self.socket_path)
                    break
            except ConnectionRefusedError:
                pass
            time.sleep(0.05)

        def recv_two_floats(sock):
            data = b''
            while len(data) < 8:  # 2 floats × 4 bytes
                packet = sock.recv(8 - len(data))
                if not packet:
                    return None, None
                data += packet
            pre, post = struct.unpack('ff', data)
            return pre, post

        try:
            while self.running:
                pre, post = recv_two_floats(client)
                if pre is None:
                    time.sleep(0.05)
                    continue
                self.pre_received.emit(pre)
                self.post_received.emit(post)
        finally:
            client.close()

    def stop(self):
        self.running = False