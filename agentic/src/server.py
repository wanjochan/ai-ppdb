from fastapi import FastAPI, WebSocket
from fastapi.staticfiles import StaticFiles
import json
import logging
import os

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI()

# 挂载静态文件目录
app.mount("/static", StaticFiles(directory="web"), name="static")

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    logger.info("WebSocket connection established")
    
    try:
        while True:
            # 接收消息
            data = await websocket.receive_text()
            message = json.loads(data)
            logger.info(f"Received message: {message}")
            
            # 简单的消息处理
            response = {
                "type": "response",
                "content": f"Received: {message.get('content', '')}"
            }
            
            # 发送响应
            await websocket.send_json(response)
            
    except Exception as e:
        logger.error(f"WebSocket error: {e}")
    finally:
        logger.info("WebSocket connection closed")

@app.get("/")
async def root():
    # 重定向到静态文件
    return {"url": "/static/index.html"}
