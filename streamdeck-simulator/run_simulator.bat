@echo off
REM StreamDeck Simulator - One-Click Launcher for Windows
REM This script starts the companion server and opens the simulator in your browser

echo ========================================
echo StreamDeck Simulator Launcher
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.8+ from https://www.python.org/
    pause
    exit /b 1
)

echo [1/3] Checking Python dependencies...
cd /d "%~dp0server"

REM Check if virtual environment exists
if not exist "venv" (
    echo Creating virtual environment...
    python -m venv venv
)

REM Activate virtual environment
call venv\Scripts\activate.bat

REM Install/update dependencies
echo Installing dependencies...
pip install -q -r requirements.txt

echo.
echo [2/3] Starting Companion Server...
echo Server URL: http://localhost:8765
echo.

REM Start server in background
start "StreamDeck Server" cmd /c "python server.py"

REM Wait for server to start
timeout /t 3 /nobreak >nul

echo [3/3] Opening Simulator...
echo.

REM Go back to simulator root
cd ..

REM Open the simulator through Flask so profile sync is allowed
start "" "http://localhost:8765/"

echo ========================================
echo StreamDeck Simulator is running!
echo ========================================
echo.
echo Controls:
echo   Ctrl+E       - Toggle Studio Mode
echo   Ctrl+S       - Save Current Profile
echo   ESC          - Close Inspector
echo.
echo The companion server is running in the background.
echo Close the server window to stop it.
echo.
pause
