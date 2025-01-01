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
import binascii

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

async def handle_config(websocket: WebSocket, data: dict):
    """处理配置消息"""
    try:
        action = data.get("action")
        config = data.get("config", {})
        
        if action == "save":
            logger.info(f"保存配置: {config}")
            
            # 验证必要的配置项
            required_fields = ["provider", "apiKey", "apiModel", "baseUrl"]
            for field in required_fields:
                if not config.get(field):
                    raise ValueError(f"缺少必要的配置项: {field}")
            
            # 根据不同的供应商设置默认值
            if config["provider"].lower() == "deepseek":
                if not config["apiModel"]:
                    config["apiModel"] = "deepseek-chat"
                if not config["baseUrl"]:
                    config["baseUrl"] = "https://api.deepseek.com"
            
            # 保存配置
            if config.get("saveLocally"):
                with open("config.json", "w", encoding="utf-8") as f:
                    json.dump(config, f, ensure_ascii=False, indent=2)
                logger.info("配置已保存到本地文件")
            
            await websocket.send_json({
                "type": "config",
                "status": "success",
                "message": "配置已保存"
            })
            
        elif action == "load":
            logger.info("加载配置")
            try:
                with open("config.json", "r", encoding="utf-8") as f:
                    saved_config = json.load(f)
                await websocket.send_json({
                    "type": "config",
                    "status": "success",
                    "config": saved_config
                })
                logger.info("配置已加载")
            except FileNotFoundError:
                logger.warning("未找到配置文件")
                await websocket.send_json({
                    "type": "config",
                    "status": "error",
                    "message": "未找到配置文件"
                })
        else:
            raise ValueError(f"未知的配置操作: {action}")
            
    except Exception as e:
        error_msg = f"处理配置时出错: {str(e)}"
        logger.error(error_msg)
        await websocket.send_json({
            "type": "config",
            "status": "error",
            "message": error_msg
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
            try:
                data = await websocket.receive_json()
                msg_type = data.get('type', '')
                logger.info(f"收到消息类型: {msg_type}")
                
                # 优先处理图片
                if msg_type == "chat" and isinstance(data.get("image"), dict):
                    logger.info("检测到图片数据")
                    try:
                        image_data = data["image"].get("data")
                        if not image_data:
                            logger.error("图片数据为空")
                            await websocket.send_json({
                                "type": "error",
                                "message": "图片数据为空"
                            })
                            continue
                        
                        # 检查数据格式
                        if not image_data.startswith('data:'):
                            logger.error("图片数据格式错误")
                            await websocket.send_json({
                                "type": "error",
                                "message": "图片数据格式错误"
                            })
                            continue
                            
                        try:
                            # 移除base64前缀
                            image_base64 = image_data.split(',')[1] if ',' in image_data else image_data
                            # 解码base64数据
                            image_bytes = base64.b64decode(image_base64)
                            logger.info(f"成功解码图片，大小: {len(image_bytes)} 字节")
                            
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
                        except binascii.Error as e:
                            error_msg = "Base64解码失败"
                            logger.error(f"{error_msg}: {str(e)}")
                            await websocket.send_json({
                                "type": "error",
                                "message": error_msg
                            })
                        except Image.UnidentifiedImageError as e:
                            error_msg = "无法识别的图片格式"
                            logger.error(f"{error_msg}: {str(e)}")
                            await websocket.send_json({
                                "type": "error",
                                "message": error_msg
                            })
                        except Exception as e:
                            error_msg = f"处理图片时出错: {str(e)}"
                            logger.error(error_msg)
                            await websocket.send_json({
                                "type": "error",
                                "message": error_msg
                            })
                    except Exception as e:
                        error_msg = f"处理图片消息时出错: {str(e)}"
                        logger.error(error_msg)
                        await websocket.send_json({
                            "type": "error",
                            "message": error_msg
                        })
                
                # 继续处理其他消息
                if msg_type == "config":
                    await handle_config(websocket, data)
                elif msg_type == "chat" and not data.get("image"):
                    await handle_chat(websocket, data)
                
            except json.JSONDecodeError as e:
                error_msg = "无效的JSON数据"
                logger.error(f"{error_msg}: {str(e)}")
                await websocket.send_json({
                    "type": "error",
                    "message": error_msg
                })
            except Exception as e:
                error_msg = f"处理消息时出错: {str(e)}"
                logger.error(error_msg)
                await websocket.send_json({
                    "type": "error",
                    "message": error_msg
                })
            
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
