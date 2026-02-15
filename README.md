# LuxTempMeter

Low-power BLE sensor node based on XIAO ESP32-C3 for:

- Ambient light (BH1750, lux)
- Temperature (DHT22, deg C)
- Humidity (DHT22, %RH)
- Battery voltage (ADC, V)

The firmware advertises measurements via BTHome v2 (BLE), then enters deep sleep for 120 seconds.

## PCB Preview

![PCB top view](screenshots/sm_white_top.png)
![PCB 3D view](screenshots/3D_PCB1_2026-02-15.png)

## Features

- Board: Seeed XIAO ESP32-C3
- BLE broadcast (BTHome v2, unencrypted service data)
- Fixed measurement cycle: read -> send -> deep sleep
- Sleep interval: 120 seconds
- PlatformIO-based firmware project

## Hardware

- MCU: Seeed XIAO ESP32-C3
- Light sensor: BH1750 (I2C)
- Temp/Humidity sensor: DHT22 (single-wire)
- Battery measurement via voltage divider to ADC

## Pinout

| Function | GPIO |
|---|---|
| I2C SDA (BH1750) | GPIO6 |
| I2C SCL (BH1750) | GPIO7 |
| Battery ADC | GPIO4 |
| DHT22 data | GPIO5 |

## Firmware Behavior

Per wake cycle (`src/main.cpp`):

1. Read BH1750 lux
2. Read battery voltage (ADC averaging)
3. Read DHT22 temperature and humidity
4. Advertise one BLE packet burst (BTHome v2)
5. Enter deep sleep for 120s

## BTHome Payload (current)

Service UUID: `0xFCD2`

- `0x05` Illuminance (lux * 100, 3 bytes)
- `0x0C` Battery voltage (mV, 2 bytes)
- `0x02` Temperature (deg C * 100, signed 2 bytes)
- `0x03` Humidity (%RH * 100, unsigned 2 bytes)

## Build and Flash (PlatformIO)

Requirements:

- PlatformIO CLI or PlatformIO IDE extension
- USB connection to XIAO ESP32-C3

Commands:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

Current environment in `platformio.ini`:

- `env:seeed_xiao_esp32c3`
- Framework: Arduino
- Build type: release (`-Os`, `CORE_DEBUG_LEVEL=0`)

## Home Assistant Notes

- Use a BLE proxy / ESPHome Bluetooth proxy if the node is far away.
- Discover as BTHome device in Home Assistant.
- Since the node sleeps most of the time, updates arrive in intervals (about every 2 minutes).

## Power and Solar Notes

This project targets low-power operation, but real solar performance depends on:

- panel size and placement
- available light over the day
- battery capacity and health
- charging IC behavior under low irradiance

If the node does not start reliably from solar only, plan for more panel headroom and test with real daily light cycles.

## Project Structure

```text
.
├── hardware/        # PCB/hardware-related files
├── screenshots/     # README images
├── src/main.cpp     # Firmware source
├── platformio.ini   # Build configuration
└── test/            # Test folder
```

## License

MIT License. See `LICENSE`.
