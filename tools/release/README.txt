NexusDeck - Companion App (no Python installation needed)
=====================================================================

QUICK START
  Windows: double-click Run-NexusDeck.bat
  Linux:   chmod +x run-nexusdeck.sh NexusDeckCompanion && ./run-nexusdeck.sh

  Then open https://nexusdeck/ (the Windows installer sets this up).
  Portable copies without that setup use http://localhost:8765/.

FULL INSTALL
  Windows: run NexusDeck-Setup-*-windows-x64.exe. It needs Administrator
  ONCE: it maps https://nexusdeck/ to this PC and trusts the self-signed
  certificate so the browser opens without a warning. Creates a desktop
  icon with the NexusDeck logo (untick in setup to skip) + optional
  start automatically at logon.
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
    Windows: Documents\NexusDeck\  (override: NEXUSDECK_DATA_DIR env var)
    Linux:   ~/.local/share/nexusdeck/
  Files: profile_state.json, server_settings.json, profiles_store.json,
  nexusdeck.crt/.key (TLS), plus state_changes.log (who changed what)
  and backups/ (auto snapshots).
  Old locations migrate there automatically on first run (newest wins).

FIRST RUN ON ESP32
  Flash the firmware (see firmware-esp32-*.bin + PlatformIO instructions
  in the main README), then on first boot join the "NexusDeck-Setup"
  Wi-Fi network to enter your home Wi-Fi + this computer's address.
  The ESP32 keeps using plain http://<this-pc>:8765.

NOTES
  - Keep this computer and the ESP32 on the same network (2.4 GHz Wi-Fi).
  - Allow inbound TCP port 8765 (and 443 for https://nexusdeck/) in the
    firewall if the ESP32 or the browser cannot connect.
  - Run only on a trusted network: the server can execute computer actions.
