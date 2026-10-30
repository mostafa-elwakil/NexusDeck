@echo off
REM NexusDeck - Release Launcher (no Python needed)
start "NexusDeck Companion" "%~dp0NexusDeckCompanion.exe"
timeout /t 3 /nobreak >nul
start "" "http://localhost:8765/"
