from fastapi import FastAPI, WebSocket
from fastapi.responses import HTMLResponse
import json
import logging
import sys
from pathlib import Path
from typing import Dict, Any

from .config import config

# 获取项目根目录
BASE_DIR = Path(__file__).resolve().parent.parent

# 配置日志
if sys.version_info >= (3, 9):
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        encoding='utf-8'
    )
else:
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s'
    )
logger = logging.getLogger(__name__)

app = FastAPI(
    title="Agentic Python",
    description="智能代理系统"
)

async def handle_config(websocket: WebSocket, message: Dict[str, Any]):
    """处理配置相关的消息"""
    action = message.get('action')
    if action == 'save':
        if message.get('data', {}).get('saveLocally'):
            config.save_local(message['data'])
            await websocket.send_json({
                "type": "response",
                "content": "配置已保存到本地"
            })
    elif action == 'load':
        local_config = config.load_local()
        if local_config:
            await websocket.send_json({
                "type": "config",
                "action": "load",
                "data": local_config
            })

async def handle_chat(websocket: WebSocket, message: Dict[str, Any]):
    """处理聊天消息"""
    # TODO: 实现与LLM的交互
    response = {
        "type": "response",
        "content": f"收到消息: {message.get('content', '')}"
    }
    await websocket.send_json(response)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    logger.info("WebSocket连接已建立")
    
    try:
        while True:
            # 接收消息
            data = await websocket.receive_text()
            message = json.loads(data)
            logger.info(f"收到消息: {message}")
            
            # 根据消息类型处理
            msg_type = message.get('type', '')
            if msg_type == 'config':
                await handle_config(websocket, message)
            elif msg_type == 'chat':
                await handle_chat(websocket, message)
            else:
                await websocket.send_json({
                    "type": "error",
                    "content": "未知的消息类型"
                })
            
    except Exception as e:
        logger.error(f"WebSocket错误: {e}")
    finally:
        logger.info("WebSocket连接已关闭")

@app.get("/")
async def root():
    """直接返回HTML内容而不是文件"""
    index_path = BASE_DIR / "web" / "index.html"
    if not index_path.exists():
        return {"error": "index.html not found"}
        
    with open(index_path, 'r', encoding='utf-8') as f:
        content = f.read()
    return HTMLResponse(content=content)
