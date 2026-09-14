# Dock Ops StreamDeck

A network-connected Stream Deck built around the ESP32-2432S028 CYD display and a Windows companion simulator.

## Features

- 12-button ESP32 touch interface in landscape mode
- XPT2046 touch input with CYD-specific SPI pins
- Wi-Fi profile synchronization every few seconds
- CPU and RAM widgets as separate buttons
- Windows actions such as opening URLs, applications, and commands
- Browser-based simulator and profile editor
- Flask companion server for ESP32 integration

## Project Structure

```text
esp32-firmware/       PlatformIO Arduino firmware for ESP32-2432S028
streamdeck-simulator/ Browser simulator, profile editor, and Flask server
esp32-micropython/    MicroPython alternative implementation
```

## Requirements

- ESP32-2432S028 / CYD board
- Windows PC on the same Wi-Fi network as the ESP32
- Python 3.8 or newer
- PlatformIO
- USB cable and an available serial port

## Start the Simulator

From the repository root, run:

```powershell
cd streamdeck-simulator
.\run_simulator.bat
```

The companion server listens on:

```text
http://localhost:8765
```

Open the simulator through that URL. The server must be running for profile synchronization and computer actions.

## Configure ESP32

Copy the example configuration and edit it with your local Wi-Fi and computer address:

```powershell
Copy-Item esp32-firmware/include/config.example.h esp32-firmware/include/config.h
```

Set these values in `esp32-firmware/include/config.h`:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL = "http://YOUR_COMPUTER_IP:8765";
```

`config.h` is ignored by Git and should never be committed.

## Build and Upload Firmware

```powershell
cd esp32-firmware
pio run
pio run -t upload --upload-port COM12
pio device monitor --baud 115200
```

Replace `COM12` with the serial port assigned to your board. Close the serial monitor before uploading.

The CYD display uses these hardware connections:

| Function | GPIO |
| --- | ---: |
| TFT MISO | 12 |
| TFT MOSI | 13 |
| TFT SCLK | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT backlight | 21 |
| Touch SCLK | 25 |
| Touch MISO | 39 |
| Touch MOSI | 32 |
| Touch CS | 33 |
| Touch IRQ | 36 |

## Use the Device

1. Start the simulator server.
2. Power the ESP32 and wait for Wi-Fi connection.
3. Open `http://localhost:8765/` in a browser.
4. Edit buttons in Studio mode and save the profile.
5. The ESP32 retrieves the updated profile automatically.
6. Touch a button on the ESP32 to execute its configured action on the companion computer.

The serial monitor prints Wi-Fi status, profile synchronization, touch coordinates, action payloads, and action responses.

## Notes

- The ESP32 and computer must be on the same network.
- Use a 2.4 GHz Wi-Fi network; ESP32 does not support 5 GHz Wi-Fi.
- Windows Firewall must allow inbound TCP traffic on port `8765`.
- Keep `esp32-firmware/include/config.h` private.
- The companion server executes configured computer actions, so run it only on a trusted network.
