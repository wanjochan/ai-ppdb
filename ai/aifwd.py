#-*- coding: utf-8 -*-

def tune_cors(app, allow_origins=['*'],
    allow_credentials=True,
    allow_methods=['*'],
    allow_headers=['*']
    ):
  from fastapi.middleware.cors import CORSMiddleware
  app.add_middleware(
      CORSMiddleware,
      allow_origins=allow_origins,
      allow_credentials=allow_credentials,
      allow_methods=allow_methods,
      allow_headers=allow_headers,
  )

def tune_gzip(app,minimum_size=9999):
  from fastapi.middleware.gzip import GZipMiddleware
  app.add_middleware(GZipMiddleware, minimum_size=9999)

from fastapi import FastAPI,Response,Request,Header
from fastapi.responses import FileResponse,StreamingResponse
#from fastapi.security import HTTPBasic, HTTPBasicCredentials

def get_fastapi_app(lifespan=None,cors=True,gzip=True):
  app = FastAPI(lifespan=lifespan)
  #from lgcFastAPI import tune_cors,tune_gzip
  tune_cors(app)
  tune_gzip(app)
  return app

import logging
from json import dumps as o2s
# e.g. OPENAI_API_KEY=`cat ../.secrets/OPENAI_API_KEY.tmp` uvicorn aifwd:app --port 7998
import os
OPENAI_API_URL_MODEL='https://api.siliconflow.cn/v1/models'
OPENAI_API_KEY=os.getenv('OPENAI_API_KEY')
# print('OPENAI_API_KEY=>',OPENAI_API_KEY)

import openai,aiohttp
# Configure OpenAI API
openai.api_key = OPENAI_API_KEY
openai.api_base = OPENAI_API_URL_MODEL

app = get_fastapi_app(cors=False,gzip=False)

@app.post("/v1/chat/completions")
async def chat_completions(request: Request):
    """处理聊天完成请求，返回流式响应
    
    请求体格式应与 OpenAI API 保持一致，包含：
    - messages: 消息列表
    - model: 模型名称（可选）
    - stream: 是否使用流式输出（必须为 True）
    """

    try:
        # 1. 获取并验证请求数据
        body = await request.json()
        print('completions body=>',body)
        messages = body.get("messages")
        if not messages:
            return {"error": "messages 不能为空"}
        
        if not body.get("stream", False):
            return {"error": "目前仅支持流式输出，stream 必须为 True"}
        
        # # 3. 创建 DeepClaude 实例
        # if not DEEPSEEK_API_KEY or not CLAUDE_API_KEY:
        #     return {"error": "未设置 API 密钥"}
            
        # deep_claude = DeepClaude(
        #     DEEPSEEK_API_KEY, 
        #     CLAUDE_API_KEY, 
        #     DEEPSEEK_API_URL,
        #     CLAUDE_API_URL,
        #     USE_OPENROUTER
        # )
        
        # 4. 返回流式响应
        return StreamingResponse(
            # TODO openai with stream of 
            openai.chat_completions_with_stream(
                messages=messages,
                model = 'deepseek-ai/DeepSeek-V3'
                #mode = 'claude-3.5-sonnet'
                #deepseek_model=DEEPSEEK_MODEL,
                #claude_model=CLAUDE_MODEL
            ),
            media_type="text/event-stream"
        )
        
    except Exception as e:
        logger.error(f"处理请求时发生错误: {e}")
        return {"error": str(e)}

@app.get("/v1/models")
async def get_models(request:Request):
    headers = {
        'Authorization': f'Bearer {OPENAI_API_KEY}',
        'Content-Type': 'application/json'
    }
    async with aiohttp.ClientSession() as session:
        try:
            async with session.get(OPENAI_API_URL_MODEL, headers=headers) as response:
                # 返回 OpenAI API 的响应
                response_text = await response.text()
                print('models=>', response_text)
                return Response(content=response_text, status_code=response.status)
                #return JSONResponse(content=fake_models, status_code=response.status)
        except aiohttp.ClientError as e:
           return {"error": str(e), "message": "Error while forwarding the request."}

    # 返回模拟的模型数据
    #return JSONResponse(content=fake_models)
@app.get('/{the_path:path}')
async def misc(request: Request, the_path=''):
    print('misc=>',the_path)
    return Response(content=o2s({"code":404,"message":f"{host}/{the_path}"}),status_code=404)