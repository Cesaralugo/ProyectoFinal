import socket
from zeroconf import ServiceInfo, Zeroconf
from PyQt6.QtCore import QThread, pyqtSignal
import json


class TcpServer(QThread):

    json_received = pyqtSignal(dict)

    def __init__(self, port=5000):
        super().__init__()
        self.port = port
        self.running = True

    def run(self):
        HOST = "0.0.0.0"

        # Obtener IP local
        temp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        temp_sock.connect(("8.8.8.8", 80))
        LOCAL_IP = temp_sock.getsockname()[0]
        temp_sock.close()

        # mDNS
        zeroconf = Zeroconf()

        service_type = "_guitarfx._tcp.local."
        service_name = "PythonGuitarFX._guitarfx._tcp.local."

        info = ServiceInfo(
            type_=service_type,
            name=service_name,
            addresses=[socket.inet_aton(LOCAL_IP)],
            port=self.port,
            properties={"desc": "Servidor PyQt"},
        )

        zeroconf.register_service(info)

        # Servidor TCP
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((HOST, self.port))
        server.listen(5)

        print("🟢 Servidor listo...")

        while self.running:
            conn, addr = server.accept()
            print(f"🔵 Conectado desde {addr}")

            while True:
                data = conn.recv(4096)
                if not data:
                    break

                try:
                    msg = data.decode().strip()
                    parsed = json.loads(msg)

                    print("📦 JSON recibido:", parsed)

                    # 🔥 Emitimos señal a la UI
                    self.json_received.emit(parsed)

                except Exception as e:
                    print("Error JSON:", e)

        zeroconf.unregister_service(info)
        zeroconf.close()
        server.close()

    def stop(self):
        self.running = False