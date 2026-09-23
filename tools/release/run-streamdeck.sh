#!/usr/bin/env bash
# DockOps StreamDeck - Release Launcher (no Python needed)
cd "$(dirname "$0")"
chmod +x ./StreamDeckCompanion 2>/dev/null
./StreamDeckCompanion &
SERVER_PID=$!
sleep 3
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "http://localhost:8765/" >/dev/null 2>&1 &
fi
echo "StreamDeck Companion running (PID $SERVER_PID). Press Ctrl+C to stop."
trap 'kill $SERVER_PID 2>/dev/null' EXIT
wait $SERVER_PID
