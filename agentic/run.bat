@echo off
echo Starting Agentic Python...

REM 检查Python环境
python --version >nul 2>&1
if errorlevel 1 (
    echo Python is not installed or not in PATH
    pause
    exit /b 1
)

REM 检查依赖
if not exist venv (
    echo Creating virtual environment...
    python -m venv venv
    call venv\Scripts\activate
    python -m pip install -r requirements.txt
) else (
    call venv\Scripts\activate
)

REM 启动服务器
start /B cmd /c "python -m uvicorn src.server:app --reload --port 8000"

REM 等待服务器启动
timeout /t 2 /nobreak >nul

REM 打开浏览器
start http://localhost:8000

echo Agentic Python is running at http://localhost:8000
echo Press Ctrl+C to stop the server
