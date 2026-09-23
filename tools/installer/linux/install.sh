#!/usr/bin/env bash
# DockOps StreamDeck installer for Linux (per-user, no sudo needed)
#
# Usage:
#   ./install.sh                        # install only
#   ./install.sh --enable-background    # also start automatically at login
#   ./install.sh --uninstall            # remove everything this installed
set -e

SRC="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/.local/share/dockops-streamdeck"
BIN="$HOME/.local/bin"
APP_DIR="$HOME/.local/share/applications"
UNIT_DIR="$HOME/.config/systemd/user"
UNIT="$UNIT_DIR/streamdeck-companion.service"

uninstall() {
    echo "Removing DockOps StreamDeck..."
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --user disable streamdeck-companion.service >/dev/null 2>&1 || true
    fi
    rm -f "$UNIT" "$APP_DIR/dockops-streamdeck.desktop" "$BIN/streamdeck"
    rm -rf "$DEST"
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --user daemon-reload >/dev/null 2>&1 || true
    fi
    echo "Uninstalled."
}

if [ "${1:-}" = "--uninstall" ]; then
    uninstall
    exit 0
fi

echo "Installing DockOps StreamDeck..."
mkdir -p "$DEST" "$BIN" "$APP_DIR"
cp "$SRC/StreamDeckCompanion" "$SRC/run-streamdeck.sh" "$DEST/"
chmod +x "$DEST/StreamDeckCompanion" "$DEST/run-streamdeck.sh"
ln -sf "$DEST/run-streamdeck.sh" "$BIN/streamdeck"
sed "s|@APP_DIR@|$DEST|g" "$SRC/dockops-streamdeck.desktop" > "$APP_DIR/dockops-streamdeck.desktop"

if [ "${1:-}" = "--enable-background" ]; then
    if ! command -v systemctl >/dev/null 2>&1; then
        echo "ERROR: systemctl not found - cannot enable background service."
        exit 1
    fi
    mkdir -p "$UNIT_DIR"
    cat > "$UNIT" <<EOF
[Unit]
Description=DockOps StreamDeck Companion Server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart="$DEST/StreamDeckCompanion"
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF
    systemctl --user daemon-reload
    systemctl --user enable streamdeck-companion.service
    echo "Background service enabled (starts automatically at login)."
fi

echo "Done. Run: streamdeck   (or open StreamDeck from the app menu)"
