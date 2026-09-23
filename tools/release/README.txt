DockOps StreamDeck - Companion App (no Python installation needed)
=====================================================================

QUICK START (portable, no install)
  Windows: double-click Run-StreamDeck.bat
  Linux:   chmod +x run-streamdeck.sh StreamDeckCompanion && ./run-streamdeck.sh

  Then open http://localhost:8765/ in your browser (opens automatically).

FULL INSTALL
  Windows: run DockOps-StreamDeck-Setup-*-windows-x64.exe (per-user, no
  admin needed). Optional: desktop icon + start automatically at logon.
  Linux:   ./install.sh [--enable-background] [--uninstall]
  installs to ~/.local (app menu entry + optional login service).

BACKGROUND MODE
  In the web UI, click the System (gear) button and turn on
  "Run in background" to start automatically at login, or off to stop it.
  Use the Quit button there to stop the server.

WHAT'S INSIDE
  StreamDeckCompanion(.exe) - the companion server, web simulator UI included.
  Your profiles/settings are saved next to the executable
  (profile_state.json, server_settings.json) and survive updates -
  just copy the new executable over the old one.

FIRST RUN ON ESP32
  Flash the firmware (see firmware-esp32-*.bin + PlatformIO instructions
  in the main README), then on first boot join the "StreamDeck-Setup"
  Wi-Fi network to enter your home Wi-Fi + this computer's address.

NOTES
  - Keep this computer and the ESP32 on the same network (2.4 GHz Wi-Fi).
  - Allow inbound TCP port 8765 in the firewall if the ESP32 can't connect.
  - Run only on a trusted network: the server can execute computer actions.
