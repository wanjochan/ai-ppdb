# PPDB API Layers

PPDB 提供三层 API 接口，以满足不同场景的需求：

## 1. C 原生接口

最底层、最高性能的接口，提供静态库和动态库。
```c
#include <ppdb/ppdb.h>

ppdb_t* db = ppdb_open("test.db");
ppdb_set(db, "key", "value");
```

### 库文件
- libppdb.so (Linux)
- libppdb.dll (Windows)
- libppdb.a (静态库)

### 适用场景
- 本地应用
- 性能敏感场景
- 需要直接内存访问的场景

## 2. Memcached 协议接口

基于 socket 的高性能网络接口，使用 async 库实现高并发。

### 服务端
```c
#include "ppdb/async.h"

// 异步处理新连接
void on_client_connect(async_handle_t* handle, int status) {
    // accept 新客户端
    async_handle_read(client, buf, len, on_command);
}

// 异步处理命令
void on_command(async_handle_t* handle, int status) {
    // 处理 memcached 命令
}
```

### 客户端示例
```python
from pymemcache.client import Client
client = Client(('localhost', 11211))
client.set('key', 'value')
```

### 特点
- 高性能二进制协议
- 异步处理所有 I/O
- 支持连接池
- 支持 pipeline

## 3. Web API 接口

RESTful HTTP 接口，提供最广泛的兼容性：

### API 端点
- GET /api/v1/get/{key}
- POST /api/v1/set
- DELETE /api/v1/delete/{key}
- POST /api/v1/batch

### 客户端示例
```python
import requests

# 同步
requests.post('http://localhost:8080/api/v1/set', 
             json={'key': 'foo', 'value': 'bar'})

# 异步
import aiohttp
async with aiohttp.ClientSession() as session:
    await session.get('http://localhost:8080/api/v1/get/foo')
```

### 特点
- RESTful 设计
- JSON 数据格式
- 跨平台兼容
- 易于集成

## 服务端内置客户端

PPDB 服务端内置了一个交互式客户端，用于直接访问和管理 PPDB 数据库：
```bash
$ ppdb_server
Starting PPDB server on port 11211...
ppdb> get mykey
"myvalue"
ppdb> set newkey 123
OK
ppdb> 
```

## 独立客户端
PPDB 提供了独立的客户端工具，用于连接和访问 PPDB 服务端：

### 命令行客户端

```bash
$ ppdb_cli
ppdb> connect localhost 11211
Connected to ppdb server.
ppdb> set test 456
OK
ppdb> get test
"456"
ppdb> help
Commands:
  get <key>
  set <key> <value>
  delete <key>
  stats
  help
  exit
```

### 编程语言接口

PPDB 提供了多种编程语言的接口，包括 C、Python 等：

#### C 接口
```c
#include <ppdb/ppdb.h>
ppdb_client_t* client = ppdb_client_connect("localhost", 11211);
ppdb_client_set(client, "key", "value");
```

#### Python 接口
```python
from ppdb import Client
client = Client('localhost', 11211)
client.set('key', 'value')
```

#### 其他语言通过 Memcached 协议接入
```javascript
const MemcachedClient = require('memcached');
const client = new MemcachedClient('localhost:11211');
client.set('key', 'value');
```

## 性能对比

| API 层          | 延迟     | 吞吐量     | 易用性 | 场景                 |
|-----------------|---------|------------|--------|---------------------|
| C 原生          | <0.1ms  | 最高      | 较难   | 本地高性能应用        |
| Memcached 协议  | ~0.5ms  | 高        | 中等   | 分布式缓存、高并发    |
| Web API        | 1-2ms   | 中等       | 最易   | 通用应用、跨平台集成   |

## 最佳实践
1. **选择合适的 API**
   - 本地高性能应用：使用 C 原生接口
   - 网络高并发场景：使用 Memcached 协议
   - 普通应用集成：使用 Web API

2. **异步处理**
   - 服务端全面采用 async 库
   - 客户端根据需求选择同步/异步 API
   - 批量操作使用 pipeline 或 batch API
