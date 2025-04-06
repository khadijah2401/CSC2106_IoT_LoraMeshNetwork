import serial
import time
import re
import threading

arduino_receiver = serial.Serial('/dev/ttyUSB0', 9600, timeout=1)  # Node 0
arduino_transmitter = serial.Serial('/dev/ttyUSB1', 9600, timeout=1)  # LoRaWAN TX

time.sleep(2)
print("✅ Listening for data from Node 0 and Transmitter...\n")

def listen_receiver():
    while True:
        if arduino_receiver.in_waiting > 0:
            raw_data = arduino_receiver.readline().decode('utf-8', errors='ignore').strip()
            print(f"[NODE 0]: {raw_data}")

            pattern = re.compile(
                r"Received from Node (\d+) \| MsgID: (\d+) \| PM2\.5: ([\d.]+) \| Temp: ([\d.]+) \| Hum: ([\d.]+) \| Time: \d+ \| Hops: ?(.*)?$"
            )
            match = pattern.search(raw_data)
            if match:
                node_id = match.group(1)
                msg_id = match.group(2)
                pm25 = str(int(float(match.group(3))))
                temp = str(int(float(match.group(4))))
                hum = str(int(float(match.group(5))))
                hops = match.group(6).strip() if match.group(6) else "-"

                hop_count = hops.count("→") + 1 if hops != "-" else 1
                formatted = f"Node {node_id} (MsgID {msg_id}): PM2.5: {pm25} Temp: {temp} Hum: {hum} Hops: {hops}"
                print(f"[PARSED]: {formatted}")

                arduino_transmitter.write((formatted + "~").encode('utf-8'))  # << terminator
                arduino_transmitter.flush()
                print(f"[FORWARDED TO TX]: {formatted}")
        time.sleep(0.05)

def watch_tx_output():
    while True:
        if arduino_transmitter.in_waiting > 0:
            line = arduino_transmitter.readline().decode('utf-8', errors='ignore').strip()
            print(f"[TX OUTPUT]: {line}")
            
            # If a ping is received from TTN downlink via transmitter
            if "PING" in line:
                print("[PI] 📡 Received PING command from TTN, forwarding to Node 0...")
                arduino_receiver.write(b"PING~")
                arduino_receiver.flush()
        time.sleep(0.05)

receiver_thread = threading.Thread(target=listen_receiver, daemon=True)
tx_thread = threading.Thread(target=watch_tx_output, daemon=True)

receiver_thread.start()
tx_thread.start()

while True:
    time.sleep(1)
