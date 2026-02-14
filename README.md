# 🌞 LuxTempMeter – ESP32 Light & Temperature Sensor sending over BLE with battery



LuxTempMeter is a small ESP32-based sensor board for measuring:

- 📊 Ambient light (lux)
- 🌡 Temperature & humidity sensor (DHT22)

- 📡 Designed for Home Assistant / IoT setups
- ⚙ Built with PlatformIO

---

## 📸 PCB Preview

![PCB Screenshot](screenshots/sm_white_top.png)



---

## 🔧 Features

- ESP32 (low power capable)
- Light sensor (BH1750 or similar)
- Temperature & humidity sensor (DHT22)
- Designed for low power 
- Compact custom PCB


---
## Idea

 using a cn3065 or similar to charge 18650 cells
---

## 📁 Project Structure 

LuxTempMeter/
├─ src/ → Firmware (PlatformIO)
├─ platformio.ini
├─ hardware/
│ └─ pcb/
│ └─ LuxTempMeter-gerber.zip
├─ screenshots/
│ └─ pcb.png
└─ README.md


---

## 🚀 Flashing

```bash
pio run -t upload




