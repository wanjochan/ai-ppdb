#!/bin/bash

echo "Starting Agentic Python..."

# 检查Python环境
if ! command -v python3 &> /dev/null; then
    echo "Python3 is not installed"
    exit 1
fi

# 检查依赖
if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
    source venv/bin/activate
    pip install -r requirements.txt
else
    source venv/bin/activate
fi

# 启动服务器
python -m uvicorn src.server:app --reload --port 18000 &
SERVER_PID=$!

# 等待服务器启动
sleep 2

# 打开浏览器
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    open http://localhost:18000
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Linux
    xdg-open http://localhost:18000 2>/dev/null || \
    sensible-browser http://localhost:18000 2>/dev/null || \
    echo "Please open http://localhost:18000 in your browser"
fi

echo "Agentic Python is running at http://localhost:18000"
echo "Press Ctrl+C to stop the server"

# 等待中断信号
trap "kill $SERVER_PID" INT
wait $SERVER_PID
