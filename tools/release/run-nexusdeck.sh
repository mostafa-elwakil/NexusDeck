#!/usr/bin/env bash
# NexusDeck - Release Launcher (no Python needed)
cd "$(dirname "$0")"
chmod +x ./NexusDeckCompanion 2>/dev/null
./NexusDeckCompanion &
SERVER_PID=$!
sleep 3
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "http://localhost:8765/" >/dev/null 2>&1 &
fi
echo "NexusDeck Companion running (PID $SERVER_PID). Press Ctrl+C to stop."
trap 'kill $SERVER_PID 2>/dev/null' EXIT
wait $SERVER_PID
