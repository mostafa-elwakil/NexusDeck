#!/usr/bin/env bash
# NexusDeck installer for Linux (per-user, no sudo needed)
#
# Usage:
#   ./install.sh                        # install only
#   ./install.sh --enable-background    # also start automatically at login
#   ./install.sh --uninstall            # remove everything this installed
set -e

SRC="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/.local/share/nexusdeck"
BIN="$HOME/.local/bin"
APP_DIR="$HOME/.local/share/applications"
UNIT_DIR="$HOME/.config/systemd/user"
UNIT="$UNIT_DIR/nexusdeck-companion.service"

desktop_file() {
    if command -v xdg-user-dir >/dev/null 2>&1; then
        xdg-user-dir DESKTOP 2>/dev/null
    else
        echo "$HOME/Desktop"
    fi
}

uninstall() {
    echo "Removing NexusDeck..."
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --user disable nexusdeck-companion.service >/dev/null 2>&1 || true
    fi
    rm -f "$UNIT" "$APP_DIR/nexusdeck.desktop" "$BIN/nexusdeck" "$BIN/streamdeck"
    rm -f "$(desktop_file)/nexusdeck.desktop"
    rm -rf "$DEST" "$HOME/.local/share/dockops-streamdeck"
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --user daemon-reload >/dev/null 2>&1 || true
    fi
    echo "Uninstalled."
}

if [ "${1:-}" = "--uninstall" ]; then
    uninstall
    exit 0
fi

echo "Installing NexusDeck..."
mkdir -p "$DEST" "$BIN" "$APP_DIR"
cp "$SRC/NexusDeckCompanion" "$SRC/run-nexusdeck.sh" "$SRC/nexusdeck.png" "$DEST/"
chmod +x "$DEST/NexusDeckCompanion" "$DEST/run-nexusdeck.sh"
ln -sf "$DEST/run-nexusdeck.sh" "$BIN/nexusdeck"
sed "s|@APP_DIR@|$DEST|g" "$SRC/nexusdeck.desktop" > "$APP_DIR/nexusdeck.desktop"
# Desktop shortcut with the app icon (best effort - needs a Desktop dir).
DESKTOP_DIR="$(desktop_file)"
if [ -n "$DESKTOP_DIR" ] && [ -d "$DESKTOP_DIR" ]; then
    cp "$APP_DIR/nexusdeck.desktop" "$DESKTOP_DIR/nexusdeck.desktop"
    chmod +x "$DESKTOP_DIR/nexusdeck.desktop"
    if command -v gio >/dev/null 2>&1; then
        gio set "$DESKTOP_DIR/nexusdeck.desktop" metadata::trusted true 2>/dev/null || true
    fi
    echo "Desktop shortcut created."
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi

if [ "${1:-}" = "--enable-background" ]; then
    if ! command -v systemctl >/dev/null 2>&1; then
        echo "ERROR: systemctl not found - cannot enable background service."
        exit 1
    fi
    mkdir -p "$UNIT_DIR"
    cat > "$UNIT" <<EOF
[Unit]
Description=NexusDeck Companion Server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart="$DEST/NexusDeckCompanion"
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF
    systemctl --user daemon-reload
    systemctl --user enable nexusdeck-companion.service
    echo "Background service enabled (starts automatically at login)."
fi

echo "Done. Run: nexusdeck   (or open NexusDeck from the app menu)"
