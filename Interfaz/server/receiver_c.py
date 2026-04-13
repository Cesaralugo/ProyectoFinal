from PyQt6.QtCore import QObject, pyqtSignal
import socket
import struct
import threading
import time
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from config import PACKET_SAMPLES, TCP_HOST, TCP_PORT, SAMPLE_DECIMATION

BATCH_SIZE = PACKET_SAMPLES
DECIMATED_SIZE = (PACKET_SAMPLES + SAMPLE_DECIMATION - 1) // SAMPLE_DECIMATION

class SocketReceiver(QObject):
    batch_received = pyqtSignal(list, list)
    pre_received   = pyqtSignal(float)
    post_received  = pyqtSignal(float)

    def __init__(self):
        super().__init__()
        self.running = False
        self.client  = None

    def start(self):
        self.running = True
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        self.client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        while self.running:
            try:
                self.client.connect((TCP_HOST, TCP_PORT))
                break
            except ConnectionRefusedError:
                time.sleep(0.1)

        BATCH_BYTES = DECIMATED_SIZE * 2 * 4

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
                raw = recv_exact(self.client, BATCH_BYTES)
                if raw is None:
                    print("[receiver] No data received, connection may have closed")
                    break
                floats     = struct.unpack(f'<{DECIMATED_SIZE * 2}f', raw)
                pre_batch  = floats[0::2]
                post_batch = floats[1::2]
                self.batch_received.emit(list(pre_batch), list(post_batch))
                self.pre_received.emit(pre_batch[0])
                self.post_received.emit(post_batch[0])
        except Exception as e:
            print(f"[receiver] Exception: {e}")
        finally:
            self.client.close()

    def send_json(self, json_data):
        if self.client is None:
            return
        try:
            msg = (json_data + "\n").encode('utf-8')
            self.client.sendall(msg)
        except (BrokenPipeError, OSError):
            print("Socket closed")

    def stop(self):
        self.running = False