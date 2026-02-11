# app.py — BridgeBrain

from flask import Flask, jsonify, send_from_directory, send_file
import threading, serial, time, json, os
import serial.tools.list_ports
from datetime import datetime
import pandas as pd
from openpyxl import load_workbook

BAUD = 9600
EXCEL_FILE = "bridge_data.xlsx"
MAX_HISTORY = 2000

app = Flask(__name__, static_url_path='', static_folder='.')

data_rows = []
data_lock = threading.Lock()


def detect_arduino_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if "Arduino" in p.description or "CH340" in p.description:
            return p.device
    return None


def append_to_excel(row):
    cols = ['Time', 'event', 'vehicleWeight', 'vibration', 'tilt',
            'vehicleCount', 'totalLoad', 'ts_ms']
    out = {k: row.get(k, '') for k in cols}

    if not os.path.exists(EXCEL_FILE):
        pd.DataFrame([out], columns=cols).to_excel(EXCEL_FILE, index=False)
        return

    try:
        wb = load_workbook(EXCEL_FILE)
        ws = wb.active
        ws.append([out[c] for c in cols])
        wb.save(EXCEL_FILE)
    except Exception:
        df_old = pd.read_excel(EXCEL_FILE)
        df_new = pd.DataFrame([out], columns=cols)
        pd.concat([df_old, df_new], ignore_index=True).to_excel(EXCEL_FILE, index=False)


def serial_listener():
    ser = None
    while True:
        try:
            if ser is None or not ser.is_open:
                port = detect_arduino_port()
                if port:
                    print(f"🔌 Auto-detect: Arduino on {port}")
                    ser = serial.Serial(port, BAUD, timeout=1)
                    print("Serial connected ✔")
                else:
                    print("⚠ Arduino not detected — retrying...")
                    time.sleep(3)
                    continue

            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue

            try:
                parsed = json.loads(line)
            except Exception:
                continue

            record = {
                'Time': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                'ts_ms': parsed.get('ts_ms', ''),
                'event': parsed.get('event', ''),
                'vehicleWeight': parsed.get('vehicleWeight', 0),
                'vibration': parsed.get('vibration', 0),
                'tilt': parsed.get('tilt', 0),
                'vehicleCount': parsed.get('vehicleCount', 0),
                'totalLoad': parsed.get('totalLoad', 0)
            }

            with data_lock:
                data_rows.append(record)
                if len(data_rows) > MAX_HISTORY:
                    data_rows.pop(0)

            append_to_excel(record)
            print("RECVD:", record)

        except Exception:
            print("⚠ Lost connection — waiting for Arduino…")
            if ser:
                try:
                    ser.close()
                except Exception:
                    pass
                ser = None
            time.sleep(3)


threading.Thread(target=serial_listener, daemon=True).start()


@app.route("/")
def index():
    return send_from_directory(".", "home.html")


@app.route("/dashboard.html")
def dash():
    return send_from_directory(".", "dashboard.html")


@app.route("/style.css")
def style():
    return send_from_directory(".", "style.css")


@app.route("/script.js")
def script():
    return send_from_directory(".", "script.js")


@app.route("/api/latest-readings")
def latest():
    with data_lock:
        return jsonify(data_rows)


@app.route("/api/download-excel")
def download_excel():
    if os.path.exists(EXCEL_FILE):
        return send_file(EXCEL_FILE, as_attachment=True)
    return jsonify({"error": "no file"}), 404


if __name__ == "__main__":
    print("🚀 Starting BridgeBrain on http://127.0.0.1:5000")
    app.run(debug=False, threaded=True, use_reloader=False)
