@echo off
REM DockOps StreamDeck - Release Launcher (no Python needed)
start "StreamDeck Companion" "%~dp0StreamDeckCompanion.exe"
timeout /t 3 /nobreak >nul
start "" "http://localhost:8765/"
