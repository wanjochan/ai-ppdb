@echo off
chcp 65001>nul

echo Starting Agentic Python...

python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python not found in PATH
    pause
    exit /b 1
)

if not exist venv (
    echo Creating virtual environment...
    python -m venv venv
)

call venv\Scripts\activate.bat
python -m pip install -r requirements.txt

start /B cmd /c "python -m uvicorn src.server:app --reload --port 8000"
timeout /t 2 /nobreak >nul
start http://localhost:8000

echo Server is running at http://localhost:8000
echo Press Ctrl+C to stop
