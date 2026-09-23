# Dock Ops StreamDeck

A network-connected Stream Deck built around the ESP32-2432S028 CYD display and a Windows companion simulator + Flask server.

## Features

- **12-button ESP32 touch interface** — fixed 4×3 grid in landscape mode, matching the web simulator exactly
- **No hardcoded Wi-Fi or server IP** — first boot (or a 2.5s hold on the status bar) opens a `StreamDeck-Setup` Wi-Fi captive portal where you enter Wi-Fi credentials, the server URL, and the background color
- **~1 second profile synchronization** — the ESP32 polls the server every second; changing a profile on either side syncs to the other automatically
- **Real drawn icons** — button icons render as vector shapes on the ESP32 screen (play, clock, camera, envelope, gear, tomato, …), with text fallback for unknown icons
- **Live widgets with real countdowns on ESP32** — timer, stopwatch, clock, uptime, CPU/RAM
- **Home page** — boots into a main screen: top bar with outside temperature, next prayer, and date (each toggleable); big digital clock; four profile buttons labeled with the real profile names; Pomodoro button plus a back (`<`) button. Every profile grid shows a `< HOME` button in the status bar — tap it (or anywhere on the status bar, or the home clock circle) to return home
- **Pomodoro timer with full control + dedicated full-screen page** — focus / short break / long break cycles, configurable durations, tap = start/pause, double-tap = reset, hold = open page (START/PAUSE, RESET, BACK); phase end triggers a **backlight blink alert**
- **Profile switching from the ESP32** — a `Switch Profile` button cycles through all profiles (or jumps to a named one) and both screens update instantly
- **Background color control** — per-profile background synced from the web, overridable from the ESP32 setup portal color picker
- **Windows actions** — open URLs/apps/commands, keyboard shortcuts (Ctrl/Alt/Shift/Win combos, function/media keys), **multi-step macros with pauses** (built in the Studio step editor, executed server-side so ESP32 buttons can run them), **Home Assistant device control** (lights, switches, scripts, scenes… via REST API), OBS Studio control (WebSocket), Docker, ping/HTTP checks, custom scripts
- **Browser simulator + Studio editor** — design buttons, live widgets, and profiles; every Apply syncs to the server and the ESP32
- **Hardened companion server** — rate limiting, CORS restricted to localhost, input validation, command-injection protection

## Project Structure

```text
esp32-firmware/       PlatformIO Arduino firmware for ESP32-2432S028
streamdeck-simulator/ Browser simulator, Studio profile editor, and Flask server
  server/             server.py, requirements.txt, profile_state.json, server_settings.json
  presets/            devops, media, obs, productivity profiles (all 4×3)
esp32-micropython/    MicroPython alternative implementation
```

## Requirements

- ESP32-2432S028 / CYD board
- Windows PC on the same network as the ESP32 (**2.4 GHz** Wi-Fi — ESP32 does not support 5 GHz)
- Python 3.8 or newer
- PlatformIO (for firmware builds)
- USB cable and an available serial port
- Windows Firewall must allow inbound TCP traffic on port `8765`

## Download (No Python Needed)

Get the latest release from **GitHub → Releases** (`dock-ops` repo):

| File | Platform | Contents |
| --- | --- | --- |
| `DockOps-StreamDeck-Setup-vX.Y.Z-windows-x64.exe` | Windows 10/11 64-bit | **Recommended installer**: per-user setup, no admin needed, optional desktop icon + start at logon |
| `DockOps-StreamDeck-vX.Y.Z-windows-x64.zip` | Windows 10/11 64-bit | Portable: `StreamDeckCompanion.exe` + launcher, UI built in |
| `DockOps-StreamDeck-vX.Y.Z-linux-x64.tar.gz` | Linux 64-bit (Ubuntu 22.04+) | Portable binary + launcher + `install.sh` installer |
| `firmware-esp32-2432S028-vX.Y.Z.bin` (+ `.sha256` files) | ESP32-2432S028 (CYD) | Ready-to-flash firmware |

1. **Windows**: run the `Setup-*.exe` installer — or extract the zip and run `Run-StreamDeck.bat`.
   **Linux**: extract the tarball and run `./install.sh` (add `--enable-background` to start at login, `--uninstall` to remove) — or just run `./run-streamdeck.sh` portably.
2. Open `http://localhost:8765/` (opens automatically).
3. Your profiles/settings are saved next to the executable and survive updates — just overwrite the old executable with the new one.

### Run in background (autostart at login)

Click the **⚙ System** button in the web toolbar and turn on **Run in background**:
- **Windows**: creates a Startup-folder shortcut (per-user, no admin). Turn it off to remove it.
- **Linux**: installs a `--user` systemd service. Turn it off to remove it.
- The **Quit** button in the same panel stops the server (relaunch it from the app menu / shortcut).

> Every push of a version tag (e.g. `git tag v1.0.0 && git push origin v1.0.0`) builds and publishes these files automatically via GitHub Actions.

## Start the Simulator (From Source)

From the repository root, run:

```powershell
cd streamdeck-simulator
.\run_simulator.bat        # Windows
```

```bash
cd streamdeck-simulator
chmod +x run_simulator.sh && ./run_simulator.sh   # Linux
```

The companion server listens on:

```text
http://localhost:8765
```

Open the simulator through that URL. The server must be running for profile synchronization and computer actions.

## Configure the ESP32 (No Code Editing Needed)

Wi-Fi credentials, the server URL, and the background color are entered through a
captive portal — `esp32-firmware/include/config.h` values are **ignored** by the firmware.

1. Flash the firmware (see below) and power the ESP32.
2. On first boot the screen shows **“WiFi Setup Needed”**.
3. On your phone/PC, connect to the **`StreamDeck-Setup`** Wi-Fi network (password: `password123`).
4. Open **`http://192.168.4.1`** and enter:
   - your home/office Wi-Fi SSID + password,
   - **Server URL**, e.g. `http://192.168.1.50:8765` (your PC’s LAN IP — find it with `ipconfig`),
   - **Background color** from the picker (or leave it to follow the active profile).
5. Save — the ESP32 restarts and connects automatically. Settings persist across reboots.

To change settings later, **hold the top status bar for ~2.5 seconds** to reopen the portal.

> If the server PC gets a new DHCP address, just reopen the portal and update the Server URL —
> or reserve a static IP for the PC in your router to avoid this entirely.

## Build and Upload Firmware

```powershell
cd esp32-firmware
pio run
pio run -t upload
pio device monitor --baud 115200
```

Close the serial monitor before uploading. Libraries (`TFT_eSPI`, `ArduinoJson`,
XPT2046 Touch, `WiFiManager`, …) are fetched automatically from `platformio.ini`.

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
2. Power the ESP32 and complete the Wi-Fi setup portal (first boot only).
3. Open `http://localhost:8765/` in a browser.
4. Press `Ctrl+E` for Studio mode, edit buttons, and click **Apply Changes** — the ESP32 updates within ~1 second.
5. Touch a button on the ESP32 to execute its configured action on the companion computer.

### Navigation gestures (ESP32)

| Gesture | Result |
| --- | --- |
| Swipe left from the right screen edge | Back to the home page |
| Tap `< HOME` (status bar) | Back to the home page |
| Tap home clock circle | Open the button grid |

Buttons now execute on release, so a swipe never triggers the button under your finger.

### Pomodoro gestures (ESP32)

| Gesture | Result |
| --- | --- |
| Tap pomodoro button | Start / pause |
| Double-tap pomodoro button | Reset session |
| Hold pomodoro button (~1s) | Open dedicated full-screen page |
| Page buttons | START/PAUSE · RESET · BACK |

When a focus/break phase ends, the **backlight blinks for ~6 seconds** as the alert
(plus the on-screen `DONE!` flash). The timer keeps running if you leave the page.

### Switching profiles from the ESP32

Assign any button the **Switch Profile (Cycle)** action type in Studio (optionally with a
target profile name). Each press switches the server profile and refreshes both screens.

The serial monitor (115200 baud) prints Wi-Fi status, profile synchronization, touch coordinates, action payloads, and action responses.

## Web Studio Quick Reference

| Shortcut / Button | Action |
| --- | --- |
| `Ctrl+E` | Toggle Studio mode |
| `Ctrl+S` | Save current profile |
| `ESC` | Close Inspector |
| Apply Changes | Save + sync to server and ESP32 instantly |
| Test Action | Execute the selected button’s action from the browser |

## Server API

Base URL: `http://<pc-ip>:8765`

| Endpoint | Method | Purpose |
| --- | --- | --- |
| `/api/health` | GET | Server health check |
| `/api/system-stats` | GET | CPU/RAM stats for widgets |
| `/api/get-profile` | GET | Active profile (ESP32 polls this; sends `X-Server-IP`) |
| `/api/set-profile` | POST | Replace the active profile |
| `/api/execute-action` | POST | Execute an action (`open_app`, `run_command`, `obs_control`, `switch_profile`, …) |
| `/api/device-status` | GET | Companion + ESP32 sync status for the web UI |
| `/api/settings` | GET/POST | Server/device settings database |
| `/api/set-background` | POST | Update active profile background color (`#rrggbb`) |
| `/api/set-esp-ip` | POST | Record the ESP32 address |
| `/api/open-app` | POST | Open an application |
| `/api/keypress` | POST | Send a keyboard shortcut, e.g. `{"keys": "ctrl+c"}` |
| `/api/ha-control` | POST | Call an HA service, e.g. `{"domain":"light","service":"turn_on","entity_id":"light.bedroom"}` |
| `/api/ha-status` | POST | Check Home Assistant connectivity and token validity |
| `/api/profiles` | GET | Ordered profile names (P1–P4 mapping for the ESP32 home page) |
| `/api/home-info` | GET | Home-page data: server time/date, outside temperature, next prayer |
| `/api/run-command` | POST | Run a shell command |
| `/api/ping` | POST | Ping a host |
| `/api/http-proxy` | POST | Proxied HTTP check |
| `/api/obs-control` | POST | OBS WebSocket actions |
| `/api/obs-status` | POST | OBS connectivity + recording state |

Persistent server files (survive restarts):

- `server/profile_state.json` — last active profile
- `server/server_settings.json` — settings DB (ESP32 IP, ports, options)
- `presets/*.json` — built-in profiles, all fixed to the 4×3 CYD layout

## Home Assistant Setup

1. In Home Assistant go to your user profile → **Security** → create a **Long-lived access token**.
2. In Studio, set any button action to **Home Assistant**. At the top of the action panel:
   - enter the **HA Server** URL (e.g. `http://192.168.1.50:8123`) and paste the token,
   - click **💾 Save server settings** (stored in `server_settings.json` on the PC only — never in profiles or git),
   - verify with **🔌 Test HA Connection**.
   - (Alternative: set `HA_URL` / `HA_TOKEN` environment variables and restart the server.)
3. Click **🔄 Load devices** to list every entity from your HA with live states, pick one from the dropdown — the Entity ID and domain fill in automatically.
4. A ready-made **Home Assistant** profile (lights, fan, plugs, scenes, lock, climate…) ships with the app — point its buttons at your own entity IDs via the device picker.

Buttons work from the web simulator, ESP32 hardware, and inside macros.

## Home Page Location Setup (Temperature + Prayer)

The home page clock and date work with no configuration. For temperature and prayer times, set your city once (free APIs, no keys needed):

```powershell
Invoke-RestMethod http://localhost:8765/api/settings -Method Post `
  -Body (@{ home_city='Cairo'; home_country='Egypt'; prayer_method=5 } | ConvertTo-Json) `
  -ContentType 'application/json'
```

Optional keys: `home_lat` / `home_lon` (skip geocoding), `prayer_method` (0–15, default 5). The ESP32 refreshes this data every minute while the home page is open.

### Home Screen Customization (Studio → Profiles → ESP32 home page)

- **Top bar widgets**: toggle Date, Prayer, and Temp individually (remaining ones spread evenly).
- **Screen brightness**: 10–100% slider ( applied instantly on the ESP32, backlight alerts still blink on top of it).

## Notes

- The ESP32 and computer must be on the same network.
- The grid is locked to 4×3 on both web and ESP32 — it cannot be resized.
- Keep `esp32-firmware/include/config.h` private (it is ignored by Git).
- The companion server executes configured computer actions, so run it only on a trusted network.
