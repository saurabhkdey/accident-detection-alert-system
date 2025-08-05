# 🚨 SmartSOS: The Life-Saving Tech You Didn’t Know You Needed

Imagine this: a lone biker takes a turn on a mountain road. The bike skids. He’s unconscious. There’s no one around.

**Now imagine this:**

Before the dust settles, his SmartSOS device has already:

* Detected the crash using motion data,
* Retrieved his exact GPS coordinates,
* Located the nearest hospital and police station,
* Texted his family with the location and emergency contacts.

**All automatically. No apps. No human intervention. Just pure, life-saving automation.**

Welcome to **SmartSOS** — your AI-powered guardian on the go.

---

## 🧠 What Is SmartSOS?

SmartSOS is a **real-time accident detection and emergency alert system** built using affordable IoT hardware, edge intelligence, and a Flask-based microservice for geolocation intelligence.

It uses:

* 🚴 **ADXL335 Accelerometer** to detect impact,
* 🌍 **NEO-6M GPS Module** to get real-time location,
* 📶 **SIM900A GSM Module** to send SMS alerts,
* 🧠 **ESP32 (Master)** and **Arduino UNO (Slave)** working in harmony over UART.

When it detects an accident, it **instantly alerts** emergency contacts and local authorities with your location and nearest help — all within seconds.

---

## 🧩 System Architecture

```
       ┌────────────┐      UART        ┌──────────────┐
       │   ESP32    │◄────────────────►│  Arduino UNO │
       └────┬───────┘                  └──────┬───────┘
            │                                 │
   ┌────────▼──────┐              ┌──────────▼──────────┐
   │ NEO-6M GPS    │              │ ADXL335 Accelerometer│
   └───────────────┘              └──────────┬──────────┘
            │                                │
       ┌────▼─────┐                 ┌────────▼────────┐
       │ Flask API│◄───────────────►│ SIM900A GSM     │
       └──────────┘    WiFi Fallback└─────────────────┘
```

---

## 🔌 Wiring at a Glance

### 🧠 ESP32 to Arduino UNO (UART)

| ESP32 GPIO  | Arduino UNO | Role    |
| ----------- | ----------- | ------- |
| GPIO22 (TX) | Pin 10 (RX) | UART TX |
| GPIO21 (RX) | Pin 11 (TX) | UART RX |
| GND         | GND         | Ground  |

### 🛰 GPS (to ESP32)

| Pin | ESP32 Pin | Description |
| --- | --------- | ----------- |
| RX  | GPIO16    | Receive     |
| TX  | GPIO17    | Transmit    |
| VCC | 5V        | Power       |
| GND | GND       | Ground      |

### 📡 GSM + Accelerometer (to Arduino)

#### SIM900A GSM

| Pin | Arduino Pin  |
| --- | ------------ |
| RX  | D3 (Soft TX) |
| TX  | D2 (Soft RX) |
| VCC | 5V (2A PSU)  |
| GND | GND          |

#### ADXL335

| Output | Arduino Pin |
| ------ | ----------- |
| X\_OUT | A0          |
| Y\_OUT | A1          |
| Z\_OUT | A2          |
| VCC    | 3.3V        |
| GND    | GND         |

---

## 🧭 Flask Server – Geolocation API

When an accident is confirmed, ESP32 makes a GET request to:

```
http://192.168.95.242:5000/get_places?latitude=XX&longitude=YY
```

Example JSON Response:

```json
{
  "hospital": {
    "name": "Mock Hospital",
    "address": "123 Test St",
    "phone": "+1 555-987-6543",
    "map_link": "https://maps.google.com/?q=31.232243,75.760773"
  },
  "police": {
    "name": "Mock Police",
    "address": "123 Test St",
    "phone": "+1 555-987-6543",
    "map_link": "https://maps.google.com/?q=31.232243,75.760773"
  },
  "location": {
    "latitude": "31.232243",
    "longitude": "75.760773",
    "map_link": "https://maps.google.com/?q=31.232243,75.760773"
  }
}
```

---

## 🔄 Full Workflow

### 1. **System Initialization**

* ESP32 starts up and verifies GPS connectivity.
* Commands Arduino UNO to:

  * Check ADXL335 (accelerometer) connection.
  * Check SIM900A GSM module.

### 2. **Accident Detection**

* Arduino continuously monitors Y and Z axis from ADXL335.
* If sudden deviation occurs:

  * Accident flag triggered.
  * Arduino notifies ESP32.

### 3. **SOS Protocol**

* ESP32 fetches live GPS coordinates.
* Makes API call to Flask server for emergency services nearby.
* Extracts and stores:

  * Police Station Info
  * Hospital Info
  * Google Maps Links
* ESP32 forwards info + location to Arduino.
* Arduino instructs GSM to SMS emergency contacts.

### 4. **Fallback Logic**

* If Arduino fails:

  * ESP32 sends emergency SMS via GSM directly.

### 5. **WiFi Fallback**

* WiFi is used **only** if GSM fails to send SMS.

### 6. **Shutdown**

* All processes terminated cleanly.
* Logs printed for debugging or forensic audit.

---

## 💬 Sample Alert Message (SMS)

```
🚨 Accident Detected!
📍 Location: https://maps.google.com/?q=31.232243,75.760773

🏥 Nearest Hospital: Mock Hospital
📞 +1 555-987-6543

🚔 Nearest Police: Mock Police
📞 +1 555-987-6543

Please respond ASAP.
```

---

## 🧠 Tech Stack

| Layer         | Tool/Module           |
| ------------- | --------------------- |
| MCU (Master)  | ESP32                 |
| MCU (Slave)   | Arduino UNO           |
| Communication | UART (GPIO UART)      |
| Sensors       | ADXL335 Accelerometer |
| Communication | SIM900A GSM           |
| Location      | NEO-6M GPS Module     |
| Backend       | Python + Flask        |
| Network       | GSM → WiFi fallback   |

---

## 📁 Project Structure

```
SmartSOS/
├── esp32_code/
│   └── master_sos.ino
├── arduino_code/
│   └── slave_accident.ino
├── flask_server/
│   └── app.py
├── assets/
│   └── wiring_diagram.png
└── README.md
```

---

## 📜 License

Made with ❤️ to save lives.
Licensed under **Creative Commons Attribution-NonCommercial-NoDerivatives 4.0** – improve it, deploy it.
