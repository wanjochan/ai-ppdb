#-*- coding: utf-8 -*-
"""
任务监控系统 - 简化版
功能：
- 提供Web界面展示任务状态
- 通过WebSocket实时更新任务状态
- 调用 tasker.py 执行任务操作
"""

from fastapi import FastAPI, WebSocket
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
import uvicorn
import os
import sys
import json
import asyncio
import subprocess
from datetime import datetime
from typing import Set, Dict, Any, List

app = FastAPI(title="任务监控系统")

# 挂载静态文件目录
static_dir = os.path.join(os.path.dirname(__file__), "static")
os.makedirs(static_dir, exist_ok=True)
app.mount("/static", StaticFiles(directory=static_dir), name="static")

# 存储活动的WebSocket连接
active_connections: Set[WebSocket] = set()

def run_tasker_command(args: List[str]) -> Dict[str, Any]:
    """执行 tasker.py 命令"""
    tasker_path = os.path.join(os.path.dirname(__file__), "tasker.py")
    cmd = [sys.executable, tasker_path] + args
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        output = result.stdout.strip()
        if not output:
            return {"status": "error", "message": "No output"}
            
        parts = output.split(":", 1)
        status_code = parts[0]
        data = parts[1] if len(parts) > 1 else None
        
        if status_code == "0" and data:  # 有任务数据
            tasks = []
            current_task = {}
            
            for line in data.split("\n"):
                if not line.strip():
                    continue
                    
                if line.startswith("ID: "):  # 新任务
                    if current_task:
                        tasks.append(current_task)
                    current_task = {}
                    current_task["id"] = line[4:].strip()
                elif ": " in line:
                    key, value = line.split(": ", 1)
                    key = key.lower().replace(" ", "_")
                    current_task[key] = value.strip()
            
            if current_task:
                tasks.append(current_task)
                
            return {"status": "has_task", "tasks": tasks}
            
        return {"status": "no_task"}
        
    except Exception as e:
        print(f"Error executing tasker command: {str(e)}")
        return {"status": "error", "message": str(e)}

async def broadcast_tasks():
    """广播任务状态到所有连接的客户端"""
    if not active_connections:
        return
        
    roles = ["User", "PM", "Dev"]
    all_tasks = {}
    
    # 获取所有角色的任务
    for role in roles:
        try:
            result = run_tasker_command(["check", "--role", role, "--include-sent", "--detail"])
            
            # 初始化该角色的任务统计
            all_tasks[role] = {
                "inbox": {
                    "unread": 0,
                    "processing": 0,
                    "completed": 0,
                    "archived": 0
                }
            }
            
            if result["status"] == "has_task":  
                for task in result.get("tasks", []):
                    if task.get("to") == role:  
                        status = task.get("status", "").lower()
                        if status in all_tasks[role]["inbox"]:
                            all_tasks[role]["inbox"][status] += 1
            
        except Exception as e:
            print(f"Error getting tasks for {role}: {str(e)}")
    
    # 广播到所有连接
    message = {
        "type": "tasks_update",
        "timestamp": datetime.now().isoformat(),
        "data": all_tasks
    }
    
    for connection in active_connections:
        try:
            await connection.send_json(message)
        except:
            active_connections.remove(connection)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket连接处理"""
    await websocket.accept()
    active_connections.add(websocket)
    
    try:
        while True:
            await websocket.receive_text()  # 接收心跳
            await broadcast_tasks()  # 更新任务状态
    except:
        active_connections.remove(websocket)

@app.get("/", response_class=HTMLResponse)
async def get_index():
    """返回监控页面"""
    with open(os.path.join(os.path.dirname(__file__), "taskmon.html"), "r", encoding="utf-8") as f:
        return f.read()

def main():
    """启动监控服务器"""
    uvicorn.run(app, host="127.0.0.1", port=18888)

if __name__ == "__main__":
    main()