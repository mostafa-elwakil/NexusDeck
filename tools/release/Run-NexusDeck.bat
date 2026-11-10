@echo off
REM NexusDeck - Release Launcher (no Python needed)
start "NexusDeck Companion" "%~dp0NexusDeckCompanion.exe"
timeout /t 3 /nobreak >nul
REM The Windows installer maps + trusts "nexusdeck"; portable copies that
REM never ran it fall back to the plain localhost URL.
findstr /I /C:"nexusdeck" "%SystemRoot%\System32\drivers\etc\hosts" >nul 2>&1
if %errorlevel%==0 (
    start "" "https://nexusdeck/"
) else (
    start "" "http://localhost:8765/"
)
