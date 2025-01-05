# PPDB HTTP API 文档

## 1. API 概述

PPDB提供RESTful HTTP API，支持数据的读写操作和集群管理。所有API都支持JSON格式的请求和响应�?
### 1.1 基本信息
- 基础URL: `http://<host>:<port>/v1`
- 内容类型: `application/json`
- 字符编码: UTF-8

### 1.2 认证方式
- 基于Token的认�?- Token在HTTP Header中通过`X-PPDB-Token`传�?
## 2. 数据操作API

### 2.1 写入数据
```http
PUT /kv/{key}
Content-Type: application/json
X-PPDB-Token: <token>

{
    "value": "base64_encoded_value",
    "ttl": 3600,  // 可选，过期时间（秒�?    "options": {  // 可选，写入选项
        "sync": true,  // 是否同步写入
        "replicas": 3  // 副本�?    }
}
```

### 2.2 读取数据
```http
GET /kv/{key}
X-PPDB-Token: <token>
```

### 2.3 删除数据
```http
DELETE /kv/{key}
X-PPDB-Token: <token>
```

### 2.4 批量操作
```http
POST /kv/batch
Content-Type: application/json
X-PPDB-Token: <token>

{
    "operations": [
        {
            "type": "put",
            "key": "key1",
            "value": "value1"
        },
        {
            "type": "delete",
            "key": "key2"
        }
    ]
}
```

## 3. 集群管理API

### 3.1 节点状�?```http
GET /cluster/status
X-PPDB-Token: <token>
```

### 3.2 添加节点
```http
POST /cluster/nodes
Content-Type: application/json
X-PPDB-Token: <token>

{
    "address": "node2:7000",
    "tags": ["storage", "compute"],
    "weight": 1
}
```

### 3.3 移除节点
```http
DELETE /cluster/nodes/{nodeId}
X-PPDB-Token: <token>
```

## 4. 监控API

### 4.1 性能指标
```http
GET /metrics
X-PPDB-Token: <token>
```

### 4.2 健康检�?```http
GET /health
```

## 5. 错误处理

### 5.1 错误响应格式
```json
{
    "error": {
        "code": "ERROR_CODE",
        "message": "错误描述",
        "details": {}
    }
}
```

### 5.2 常见错误�?- 400: 请求参数错误
- 401: 未认�?- 403: 无权�?- 404: 资源不存�?- 409: 资源冲突
- 500: 服务器内部错�?
## 6. 最佳实�?
### 6.1 重试策略
- 使用指数退避算�?- 设置最大重试次�?- 处理幂等�?
### 6.2 批量操作
- 合理设置批量大小
- 处理部分失败情况
- 实现原子性保�?