from fastapi import FastAPI, WebSocket
from fastapi.staticfiles import StaticFiles
import json
import logging
import os

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    encoding='utf-8'  # 添加utf-8编码支持
)
logger = logging.getLogger(__name__)

app = FastAPI(
    title="Agentic Python",
    description="智能代理系统"
)

# 挂载静态文件目录
app.mount("/static", StaticFiles(directory="web"), name="static")

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
    # 重定向到静态文件
    return {"url": "/static/index.html"}
