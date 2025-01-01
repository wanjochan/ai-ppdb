from fastapi import FastAPI, WebSocket
from fastapi.responses import HTMLResponse
import json
import logging
import sys
from pathlib import Path
from typing import Dict, Any
from PIL import Image
import io
import base64

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
    try:
        content = message.get("content", "")
        config = message.get("config", {})
        
        # TODO: 这里添加与AI服务商的交互逻辑
        
        await websocket.send_json({
            "type": "chat_response",
            "content": f"收到消息: {content}"
        })
        
    except Exception as e:
        error_msg = f"处理消息时出错: {str(e)}"
        logger.error(error_msg)
        await websocket.send_json({
            "type": "error",
            "content": error_msg
        })

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    logger.info("WebSocket连接已建立")
    
    try:
        while True:
            data = await websocket.receive_json()
            logger.info(f"收到消息类型: {data.get('type')}")
            
            if data.get("type") == "chat" and "image" in data:
                logger.info("检测到图片数据")
                image_data = data["image"]["data"]
                try:
                    # 移除base64前缀
                    image_base64 = image_data.split(',')[1] if ',' in image_data else image_data
                    # 解码base64数据
                    image_bytes = base64.b64decode(image_base64)
                    # 使用PIL打开图片获取信息
                    with Image.open(io.BytesIO(image_bytes)) as img:
                        image_info = {
                            "format": img.format,
                            "size": f"{len(image_bytes) / 1024:.1f}KB",
                            "dimensions": f"{img.width}x{img.height}",
                            "mode": img.mode
                        }
                        logger.info(f"成功处理图片: {image_info}")
                        await websocket.send_json({
                            "type": "image_info",
                            "info": image_info
                        })
                except Exception as e:
                    error_msg = f"处理图片时出错: {str(e)}"
                    logger.error(error_msg)
                    await websocket.send_json({
                        "type": "image_info",
                        "info": {"error": error_msg}
                    })
            
            # 继续处理消息
            if data.get("type") == "config":
                await handle_config(websocket, data)
            elif data.get("type") == "chat":
                await handle_chat(websocket, data)
            
    except WebSocketDisconnect:
        logger.info("Client disconnected")
    except Exception as e:
        logger.error(f"WebSocket错误: {str(e)}")
        await websocket.close()

@app.get("/")
async def root():
    """直接返回HTML内容而不是文件"""
    index_path = BASE_DIR / "web" / "index.html"
    if not index_path.exists():
        return {"error": "index.html not found"}
        
    with open(index_path, 'r', encoding='utf-8') as f:
        content = f.read()
    return HTMLResponse(content=content)
