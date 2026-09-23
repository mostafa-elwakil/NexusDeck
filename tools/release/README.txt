NexusDeck - Companion App (no Python installation needed)
=====================================================================

QUICK START (portable, no install)
  Windows: double-click Run-NexusDeck.bat
  Linux:   chmod +x run-nexusdeck.sh NexusDeckCompanion && ./run-nexusdeck.sh

  Then open http://localhost:8765/ in your browser (opens automatically).

FULL INSTALL
  Windows: run NexusDeck-Setup-*-windows-x64.exe (per-user, no
  admin needed). Optional: desktop icon + start automatically at logon.
  Linux:   ./install.sh [--enable-background] [--uninstall]
  installs to ~/.local (app menu entry + optional login service).

BACKGROUND MODE
  In the web UI, click the System (gear) button and turn on
  "Run in background" to start automatically at login, or off to stop it.
  Use the Quit button there to stop the server.

WHAT'S INSIDE
  NexusDeckCompanion(.exe) - the companion server, web simulator UI included.
  Logs go to companion.log in the same state folder below.
  Your profiles/settings live in ONE shared per-user folder and survive
  updates and restarts, no matter which copy you run (installed or dev):
    Windows: %APPDATA%\NexusDeck\  (profile_state.json, server_settings.json)
    Linux:   ~/.local/share/nexusdeck/
  Old copies migrate their files there automatically on first run.

FIRST RUN ON ESP32
  Flash the firmware (see firmware-esp32-*.bin + PlatformIO instructions
  in the main README), then on first boot join the "NexusDeck-Setup"
  Wi-Fi network to enter your home Wi-Fi + this computer's address.

NOTES
  - Keep this computer and the ESP32 on the same network (2.4 GHz Wi-Fi).
  - Allow inbound TCP port 8765 in the firewall if the ESP32 can't connect.
  - Run only on a trusted network: the server can execute computer actions.
