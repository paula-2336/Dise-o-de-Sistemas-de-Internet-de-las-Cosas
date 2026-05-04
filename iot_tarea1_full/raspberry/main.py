
import asyncio
import json
import csv
import time
import struct
import numpy as np
from bleak import BleakClient, BleakScanner
from PyQt5 import QtWidgets, QtCore
import pyqtgraph as pg

# UUIDs 
ACCEL_CHAR_UUID = "00002a58-0000-1000-8000-00805f9b34fb"
TEMP_CHAR_UUID = "0000246e-0000-1000-8000-00805f9b34fb"

class IoTApp(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("IoT Real-Time Dashboard")
        self.data_accel = {"t": [], "ax": [], "ay": [], "az": []}
        self.last_temp = None
        self.last_phone = None
        self.is_logging = False
        self.csv_file = None
        self.init_ui()

    def init_ui(self):
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)
        layout = QtWidgets.QVBoxLayout(central_widget)

        # Selection
        self.cb_accel = QtWidgets.QCheckBox("Acelerómetro (ESP32)")
        self.cb_temp = QtWidgets.QCheckBox("Temperatura (ESP32)")
        self.cb_phone = QtWidgets.QCheckBox("Smartphone")
        layout.addWidget(self.cb_accel)
        layout.addWidget(self.cb_temp)
        layout.addWidget(self.cb_phone)

        # Graph
        self.graph = pg.PlotWidget(title="Acelerómetro (G)")
        self.graph.addLegend()
        self.curve_x = self.graph.plot(pen='r', name="Ax")
        self.curve_y = self.graph.plot(pen='g', name="Ay")
        self.curve_z = self.graph.plot(pen='b', name="Az")
        layout.addWidget(self.graph)

        # Labels
        self.lbl_temp = QtWidgets.QLabel("Temp: --")
        self.lbl_phone = QtWidgets.QLabel("Phone: --")
        layout.addWidget(self.lbl_temp)
        layout.addWidget(self.lbl_phone)
        
        # Stats
        self.lbl_stats = QtWidgets.QLabel("Stats (RMS): --")
        layout.addWidget(self.lbl_stats)

        # Log Button
        self.btn_log = QtWidgets.QPushButton("Start Logging")
        self.btn_log.clicked.connect(self.toggle_logging)
        layout.addWidget(self.btn_log)

    def toggle_logging(self):
        self.is_logging = not self.is_logging
        if self.is_logging:
            self.btn_log.setText("Stop Logging")
            self.csv_file = open("iot_log.csv", "w", newline="")
            self.csv_writer = csv.writer(self.csv_file)
            self.csv_writer.writerow(["timestamp", "source", "ax", "ay", "az", "temp", "phone"])
        else:
            self.btn_log.setText("Start Logging")
            if self.csv_file: self.csv_file.close()

    def update_accel(self, ax, ay, az, ts):
        if not self.cb_accel.isChecked(): return
        self.data_accel["t"].append(ts)
        self.data_accel["ax"].append(ax)
        self.data_accel["ay"].append(ay)
        self.data_accel["az"].append(az)
        
        # Windowing (2 seconds ~ 2000 samples)
        if len(self.data_accel["t"]) > 2000:
            for k in self.data_accel: self.data_accel[k].pop(0)

        self.curve_x.setData(self.data_accel["ax"])
        self.curve_y.setData(self.data_accel["ay"])
        self.curve_z.setData(self.data_accel["az"])
        
        if len(self.data_accel["ax"]) >= 1000:
            win = np.array(self.data_accel["ax"][-1000:])
            rms = np.sqrt(np.mean(win**2))
            self.lbl_stats.setText(f"RMS Ax (1000 samples): {rms:.2f}")

        if self.is_logging:
            self.csv_writer.writerow([ts, "ESP32_Accel", ax, ay, az, "", ""])

    def update_temp(self, temp, ts):
        if not self.cb_temp.isChecked(): return
        self.lbl_temp.setText(f"Temp: {temp:.2f} C (TS: {ts})")
        if self.is_logging:
            self.csv_writer.writerow([ts, "ESP32_Temp", "", "", "", temp, ""])

async def run_ble_client(app):
    # Load config
    with open("config.json") as f:
        config = json.load(f)

    def accel_handler(sender, data):
        # Format: uint32 (4) + 3*float32 (12) = 16 bytes
        ts, ax, ay, az = struct.unpack("<Ifff", data)
        app.update_accel(ax, ay, az, ts)

    def temp_handler(sender, data):
        # Format: uint32 (4) + float32 (4) = 8 bytes
        ts, temp = struct.unpack("<If", data)
        app.update_temp(temp, ts)

    async with BleakClient(config["esp32_address"]) as client:
        await client.start_notify(ACCEL_CHAR_UUID, accel_handler)
        await client.start_notify(TEMP_CHAR_UUID, temp_handler)
        while True:
            await asyncio.sleep(1)

if __name__ == "__main__":
    import sys
    qt_app = QtWidgets.QApplication(sys.argv)
    window = IoTApp()
    window.show()
    
    loop = asyncio.get_event_loop()
    sys.exit(qt_app.exec_())
