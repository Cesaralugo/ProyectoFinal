import sys
from PyQt6.QtWidgets import QApplication
from ui.main_window import MainWindow
from server.receiver_app import TcpServer

app = QApplication(sys.argv)
window = MainWindow()
window.show()
sys.exit(app.exec())
