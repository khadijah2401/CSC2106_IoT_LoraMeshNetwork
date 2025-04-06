# LoRa Mesh Network with Encrypted Payload and ThingsBoard Integration

This project implements a scalable and secure IoT mesh network using Arduino nodes with LoRa radios. Sensor data is encrypted and transmitted across nodes and finally forwarded to The Things Network (TTN) via a Raspberry Pi bridge. Data is visualized in real-time on a ThingsBoard dashboard.

## 📡 Project Overview

- Nodes use RH_RF95 for mesh communication.
- AES-128 encryption for secure payload transmission.
- Raspberry Pi acts as a serial bridge to TTN via LMIC uplink.
- TTN forwards data to ThingsBoard via webhook integration.
- PING-PONG downlink testing supported via TTN + ThingsBoard.

---

## 🔧 Components

### Hardware
- Arduino Uno (LoRa Mesh Nodes)
- Raspberry Pi (Mesh Collector + Serial Parser)
- WisGate Edge Lite 2 (LoRaWAN Gateway)
- LoRa RFM95 modules

### Software
- Arduino IDE
- ThingsBoard Cloud
- The Things Network (TTN)
- LMIC library (LoRaWAN uplink/downlink)
- cppQueue (buffering)
- AESLib (encryption)
- Python script (serial read on Pi)

---

## 🔗 File Structure

| File                     | Description                            |
|--------------------------|----------------------------------------|
| `node1_ping.ino`         | Collector node (Node 1) with ping handling |
| `node2_low_stable.ino`   | Stable node with low values            |
| `node3_low.ino`          | Node with random low PM2.5 data        |
| `node4_high_stable.ino`  | Stable node with high values           |
| `node5_high.ino`         | Node with random high PM2.5 data       |
| `receiver_ping.ino`      | Node 0 receiver/decryptor              |
| `transmitter_ping.ino`   | Uplink transmitter via LMIC            |
| `serial_script.py`       | Python parser on Pi to forward data    |

---

## ⚙️ Features

- Flood-based mesh forwarding with duplicate filtering
- AES encryption of `Payload` struct before transmission
- Uplink buffer queue on transmitter
- Downlink command `0x01` triggers ping from TTN
- Latency calculation and hop tracking
- ThingsBoard table widget displays sensor info and system status

---

## 🚀 Setup Instructions

1. Flash the respective `.ino` files to each node using Arduino IDE.
2. Set up Raspberry Pi to run `serial_script.py` for parsing mesh output.
3. Deploy WisGate and configure TTN with correct keys.
4. Use ThingsBoard Rule Chains to decode uplink, handle downlink, and visualize data.
5. Send `PING` from ThingsBoard Cloud Dashboard to test latency tracking and hop count.

---

## 📈 Future Improvements

- Dynamic routing based on RSSI
- OTA firmware updates
- Larger queue with priority-based flushing
- Real-time geolocation with GPS shields
- Adaptive data rate (ADR) on LoRa

---

## 📬 Contact
For help: 
- 2301124@sit.singaporetech.edu.sg (Nur Khadijah Afreen Binte Mohamed Jahabar Sadik)
- 2300892@sit.singaporetech.edu.sg (Choo Jin Quan)
- 2303340@sit.singaporetech.edu.sg (Toh Hao Yi)
- 2303403@sit.singaporetech.edu.sg(Ignatius Lim Zheng Liang)
- 2301119@sit.singaporetech.edu.sg (Tan Ting Xuan, Daryl)

Developed for academic and experimental purposes.