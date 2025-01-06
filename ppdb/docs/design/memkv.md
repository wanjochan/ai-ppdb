# MemKV 设计文档

## 1. 概述

MemKV 是 PPDB 的一个内存键值存储模式，提供高性能的内存数据存储和访问能力。

### 1.1 设计目标

- 高性能：利用内存存储，提供快速的读写操作
- 多协议支持：兼容多种流行的 KV 存储协议
- 可扩展：支持后续添加更多功能和协议
- 可靠性：提供基本的内存管理和数据保护机制

## 2. 核心组件

### 2.1 数据结构

- 使用跳表（skiplist）作为核心数据结构
- 支持快速的查找、插入和删除操作
- 内置内存池管理，支持大小数据优化

### 2.2 协议支持

目前支持三种协议：

1. Memcached 协议
   - 文本协议，易于调试
   - 支持基本的 get/set/delete 操作
   - 兼容现有的 memcached 客户端

2. Redis 协议（RESP）
   - 支持二进制安全的数据传输
   - 实现基本的字符串操作
   - 兼容现有的 redis 客户端

3. 二进制协议
   ```
   请求格式：
   +--------+--------+--------+--------+
   |  Magic | OpCode |  Flag  | Status |
   +--------+--------+--------+--------+
   |            Key Length            |
   +--------+--------+--------+--------+
   |           Value Length           |
   +--------+--------+--------+--------+
   |              CAS ID             |
   +--------+--------+--------+--------+
   |             Key Data            |
   +--------+--------+--------+--------+
   |            Value Data           |
   +--------+--------+--------+--------+
   ```

### 2.3 内存管理

- 小数据优化：32字节以内的数据直接存储
- 大数据处理：使用扩展内存存储
- 内存限制：支持配置最大内存使用
- 驱逐策略：TODO（计划支持 LRU/LFU）

## 3. 接口设计

### 3.1 命令行接口

```bash
# 启动服务器
ppdb server --mode=memkv --protocol=memcached --port=11211
ppdb server --mode=memkv --protocol=redis --port=6379
ppdb server --mode=memkv --protocol=binary --port=9527
```

### 3.2 编程接口

```c
// 创建实例
ppdb_error_t ppdb_memkv_create(ppdb_memkv_t* kv, const ppdb_options_t* options);

// 基本操作
ppdb_error_t ppdb_memkv_get(ppdb_memkv_t kv, const ppdb_data_t* key, ppdb_data_t* value);
ppdb_error_t ppdb_memkv_put(ppdb_memkv_t kv, const ppdb_data_t* key, const ppdb_data_t* value);
ppdb_error_t ppdb_memkv_delete(ppdb_memkv_t kv, const ppdb_data_t* key);
```

## 4. 后续计划

1. 内存管理优化
   - 实现内存限制和监控
   - 添加数据驱逐机制
   - 优化内存分配策略

2. 协议增强
   - 完善协议实现
   - 添加更多命令支持
   - 优化协议解析性能

3. 性能优化
   - 实现分片存储
   - 优化并发控制
   - 添加性能监控 