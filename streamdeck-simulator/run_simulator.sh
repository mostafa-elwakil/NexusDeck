#!/usr/bin/env bash
# NexusDeck Simulator - One-Click Launcher for Linux
# Starts the companion server (venv) and opens the simulator in your browser
set -e

echo "========================================"
echo "NexusDeck Simulator Launcher"
echo "========================================"
echo

if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: Python 3 is not installed or not in PATH"
    echo "Install it with: sudo apt install python3 python3-venv python3-pip"
    exit 1
fi

echo "[1/3] Checking Python dependencies..."
cd "$(dirname "$0")/server"

if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
fi

# shellcheck disable=SC1091
source venv/bin/activate

echo "Installing dependencies..."
pip install -q -r requirements.txt

echo
echo "[2/3] Starting Companion Server..."
echo "Server URL: http://localhost:8765"
echo

python3 server.py &
SERVER_PID=$!

sleep 3

echo "[3/3] Opening Simulator..."
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "http://localhost:8765/" >/dev/null 2>&1 &
fi

echo "========================================"
echo "NexusDeck Simulator is running!"
echo "========================================"
echo
echo "Controls:"
echo "  Ctrl+E       - Toggle Studio Mode"
echo "  Ctrl+S       - Save Current Profile"
echo "  ESC          - Close Inspector"
echo
echo "Press Ctrl+C to stop the server."
echo

trap 'kill $SERVER_PID 2>/dev/null' EXIT
wait $SERVER_PID
