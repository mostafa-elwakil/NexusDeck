DockOps StreamDeck - Companion App (no Python installation needed)
=====================================================================

QUICK START
  Windows: double-click Run-StreamDeck.bat
  Linux:   chmod +x run-streamdeck.sh StreamDeckCompanion && ./run-streamdeck.sh

  Then open http://localhost:8765/ in your browser (opens automatically).

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
