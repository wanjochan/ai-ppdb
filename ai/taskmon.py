#-*- coding: utf-8 -*-
"""
任务监控系统

功能：
- 提供Web界面展示任务状态
- 通过WebSocket实时更新任务状态
- 调用 tasker.py 执行任务操作
"""

from fastapi import FastAPI, HTTPException, WebSocket
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse
import uvicorn
import os
import sys
import json
import asyncio
import subprocess
from datetime import datetime
from typing import Set, Dict, Any, Optional, List
from enum import Enum
from pydantic import BaseModel

class TaskStatus(str, Enum):
    """任务状态枚举"""
    UNREAD = "unread"
    PROCESSING = "processing"
    REPLIED = "replied" 
    COMPLETED = "completed"
    ARCHIVED = "archived"
    RECALLED = "recalled"

app = FastAPI(title="任务监控系统")

# 挂载静态文件目录
static_dir = os.path.join(os.path.dirname(__file__), "static")
os.makedirs(static_dir, exist_ok=True)
app.mount("/static", StaticFiles(directory=static_dir), name="static")

# 存储活动的WebSocket连接
active_connections: Set[WebSocket] = set()

# 上次检查的变更ID
last_change_id = 0

def run_tasker_command(args: List[str]) -> Dict[str, Any]:
    """执行 tasker.py 命令"""
    tasker_path = os.path.join(os.path.dirname(__file__), "tasker.py")
    cmd = [sys.executable, tasker_path] + args
    print(f"Executing command: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print(f"Command output: {result.stdout}")
        return json.loads(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Command failed with error: {e.stderr}")
        if e.stdout:
            try:
                return json.loads(e.stdout)
            except:
                pass
        raise ValueError(f"Command failed: {e.stderr}")
    except Exception as e:
        print(f"Exception: {str(e)}")
        raise ValueError(f"Failed to execute command: {str(e)}")

def check_changes() -> bool:
    """检查是否有新的变更"""
    global last_change_id
    try:
        result = run_tasker_command(["--mode", "check-changes", "--last-id", str(last_change_id)])
        if result["status"] == "has_changes":
            last_change_id = result["last_id"]
            return True
        return False
    except Exception as e:
        print(f"Error checking changes: {str(e)}")
        return False

async def broadcast_tasks():
    """广播任务状态到所有连接的客户端"""
    if not active_connections:
        return
        
    # 检查是否有变更
    if not check_changes():
        return
        
    roles = ["User", "PM", "Dev"]
    all_tasks = {}
    
    # 获取所有角色的任务
    for role in roles:
        try:
            print(f"Getting tasks for role: {role}")
            result = run_tasker_command(["--mode", "check", "--role", role, "--include-sent"])
            print(f"Result for {role}: {result}")
            if result["status"] == "has_task":
                all_tasks[role] = result["tasks"]
            else:
                all_tasks[role] = []
        except Exception as e:
            print(f"Error getting tasks for {role}: {str(e)}")
            all_tasks[role] = []
    
    # 广播到所有连接
    message = {
        "type": "tasks_update",
        "timestamp": datetime.now().isoformat(),
        "data": all_tasks
    }
    print(f"Broadcasting message: {message}")
    
    for connection in active_connections:
        try:
            await connection.send_json(message)
            print(f"Message sent to connection")
        except:
            print(f"Failed to send message to connection")
            active_connections.remove(connection)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket连接处理"""
    await websocket.accept()
    active_connections.add(websocket)
    
    try:
        while True:
            # 等待客户端消息（心跳检测）
            await websocket.receive_text()
    except:
        active_connections.remove(websocket)

@app.on_event("startup")
async def startup_event():
    """启动时开始任务状态广播任务"""
    asyncio.create_task(periodic_broadcast())

async def periodic_broadcast():
    """定期检查变更并广播任务状态"""
    while True:
        await broadcast_tasks()
        await asyncio.sleep(1)  # 每秒检查一次变更

@app.get("/", response_class=HTMLResponse)
async def get_index():
    """返回监控页面"""
    with open(os.path.join(os.path.dirname(__file__), "taskmon.html")) as f:
        return f.read()

@app.get("/api/roles")
async def get_roles() -> Dict[str, List[str]]:
    """获取所有角色"""
    return {
        "status": "success",
        "data": ["User", "PM", "Dev"]
    }

# API Models
class TaskCreate(BaseModel):
    from_role: str
    to_role: str
    subject: str
    content: str
    reply_to: Optional[str] = None

class TaskRecall(BaseModel):
    from_role: str

class TaskStatusUpdate(BaseModel):
    role: str
    new_status: TaskStatus
    note: Optional[str] = None

# API Endpoints
@app.post("/api/tasks/send")
async def send_task(task: TaskCreate):
    """发送任务"""
    try:
        result = run_tasker_command([
            "--mode", "send",
            "--from-role", task.from_role,
            "--to-role", task.to_role,
            "--subject", task.subject,
            "--content", task.content
        ] + (["--reply-to", task.reply_to] if task.reply_to else []))
        
        await broadcast_tasks()
        return result
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@app.post("/api/tasks/{task_id}/recall")
async def recall_task(task_id: str, recall: TaskRecall):
    """撤回任务"""
    try:
        result = run_tasker_command([
            "--mode", "recall",
            "--from-role", recall.from_role,
            "--task-id", task_id
        ])
        
        await broadcast_tasks()
        return result
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@app.post("/api/tasks/{task_id}/status")
async def update_task_status(task_id: str, update: TaskStatusUpdate):
    """更新任务状态"""
    try:
        result = run_tasker_command([
            "--mode", "update-status",
            "--role", update.role,
            "--task-id", task_id,
            "--status", update.new_status,
        ] + (["--note", update.note] if update.note else []))
        
        await broadcast_tasks()
        return result
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

@app.get("/api/tasks")
async def get_tasks(role: str, status: Optional[TaskStatus] = None, include_sent: bool = False):
    """获取任务列表"""
    try:
        args = ["--mode", "check", "--role", role]
        if status:
            args.extend(["--status", status])
        if include_sent:
            args.append("--include-sent")
        
        return run_tasker_command(args)
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

def main():
    """启动监控服务器"""
    uvicorn.run(app, host="127.0.0.1", port=18888)

if __name__ == "__main__":
    main() 