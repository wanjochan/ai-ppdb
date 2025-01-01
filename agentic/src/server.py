from fastapi import FastAPI, WebSocket
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse, RedirectResponse
import json
import logging
import os
import sys
from pathlib import Path

# 获取项目根目录
BASE_DIR = Path(__file__).resolve().parent.parent

# 配置日志
if sys.version_info >= (3, 9):
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        encoding='utf-8'  # Python 3.9+ 支持
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

# 挂载静态文件目录
app.mount("/static", StaticFiles(directory=str(BASE_DIR / "web")), name="static")

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
            
            # 简单的消息处理
            response = {
                "type": "response",
                "content": f"收到消息: {message.get('content', '')}"
            }
            
            # 发送响应
            await websocket.send_json(response)
            
    except Exception as e:
        logger.error(f"WebSocket错误: {e}")
    finally:
        logger.info("WebSocket连接已关闭")

@app.get("/")
async def root():
    index_path = BASE_DIR / "web" / "index.html"
    if index_path.exists():
        return FileResponse(str(index_path))
    return {"error": "index.html not found"}
