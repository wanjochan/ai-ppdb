# PPDB WAL 设计

> 本文档详细说明了 PPDB 的 WAL（Write Ahead Log）设计和实现。它是存储引擎的持久化日志，负责保证数据的可靠性和一致性。相关文档：
> - MemTable 设计见 `design/memtable.md`
> - SSTable 设计见 `design/sstable.md`
> - 整体设计见 `overview/DESIGN.md`

# WAL (Write-Ahead Logging) 实现文档

## 1. 概述

WAL是PPDB中的一个关键组件，用于确保数据的持久性和一致性。它通过在执行实际的数据修改之前，先将修改记录写入日志文件，从而在系统崩溃时能够恢复数据。

### 1.1 设计目标

- 确保数据修改的持久性
- 支持崩溃恢复
- 跨平台兼容性
- 高性能和低延迟
- 线程安全

### 1.2 配置管理

WAL的配置目前通过 `ppdb_wal_config_t` 结构体在代码级别进行管理：

```c
typedef struct ppdb_wal_config {
    char dir_path[PPDB_MAX_PATH_SIZE];  // WAL目录路径
    char filename[PPDB_MAX_PATH_SIZE];  // WAL文件名
    size_t segment_size;                // 段大小
    size_t max_segments;                // 最大段数量
    size_t max_total_size;             // 最大总大小
    size_t max_records;                // 最大记录数量
    bool sync_write;                   // 是否同步写入
    bool use_buffer;                   // 是否使用缓冲区
    size_t buffer_size;                // 缓冲区大小
    ppdb_compression_t compression;     // 压缩算法
    bool enable_compression;           // 启用压缩选项
} ppdb_wal_config_t;
```

配置项分为以下几类：

1. 基础配置：
   - `dir_path`：WAL目录路径
   - `filename`：WAL文件名
   - `segment_size`：段大小
   - `max_segments`：最大段数量
   - `max_total_size`：最大总大小
   - `max_records`：最大记录数量

2. 性能相关配置：
   - `sync_write`：是否同步写入（影响持久性和性能）
   - `use_buffer`：是否使用缓冲区
   - `buffer_size`：缓冲区大小

3. 压缩相关配置：
   - `compression`：压缩算法
   - `enable_compression`：是否启用压缩

当前配置在代码中通过构造配置结构体来设置：

```c
ppdb_wal_config_t config = {
    .dir_path = "/path/to/wal",
    .segment_size = 64 * 1024 * 1024,  // 64MB
    .max_segments = 10,
    .sync_write = true,
    .use_buffer = true,
    .buffer_size = 4096,
    .enable_compression = false
};

ppdb_wal_t* wal;
ppdb_error_t err = ppdb_wal_create(&config, &wal);
```

## 2. 数据结构

### 2.1 WAL文件头
```c
struct ppdb_wal_header_t {
    uint32_t magic;           // 魔数(PWAL)
    uint32_t version;         // 版本号
    uint32_t segment_size;    // 段大小
    uint32_t reserved;        // 保留字段
};
```

### 2.2 WAL记录头
```c
struct ppdb_wal_record_header_t {
    uint32_t crc32;          // CRC32校验和
    uint32_t type;           // 记录类型
    uint32_t key_size;       // 键大小
    uint32_t value_size;     // 值大小
    uint32_t reserved;       // 保留字段
};
```

### 2.3 记录类型
```c
typedef enum {
    PPDB_WAL_PUT,     // 插入/更新操作
    PPDB_WAL_DELETE   // 删除操作
} ppdb_wal_record_type_t;
```

## 3. 关键实现

### 3.1 写入流程

1. 准备记录头
   - 设置记录类型
   - 设置键值大小
   - 计算CRC32校验和

2. 写入数据
   - 写入记录头
   - 写入键
   - 写入值（如果有）
   - 同步到磁盘（如果启用sync_write）

```c
// CRC32计算示例
uint32_t crc = 0;
crc = crc32_z(crc, (uint8_t*)&header.type, sizeof(header.type));
crc = crc32_z(crc, (uint8_t*)&header.key_size, sizeof(header.key_size));
crc = crc32_z(crc, (uint8_t*)&header.value_size, sizeof(header.value_size));
crc = crc32_z(crc, key, key_len);
if (value && value_len > 0) {
    crc = crc32_z(crc, value, value_len);
}
```

### 3.2 恢复流程

1. 扫描WAL目录
2. 按顺序读取WAL文件
3. 验证每条记录
   - 检查文件头魔数和版本
   - 验证CRC32校验和
4. 重放操作到MemTable

### 3.3 错误处理

- 文件I/O错误：返回PPDB_ERR_IO
- 数据损坏：返回PPDB_ERR_CORRUPTED
- 内存不足：返回PPDB_ERR_OUT_OF_MEMORY
- 参数无效：返回PPDB_ERR_INVALID_ARG

## 4. 跨平台兼容性

### 4.1 文件操作
- 使用Cosmopolitan提供的跨平台API
- 统一使用UNIX风格的路径分隔符
- 处理不同平台的文件权限

### 4.2 字节序
- 所有整数字段使用本地字节序
- CRC32计算考虑字节序一致性

## 5. 并发控制

### 5.1 互斥锁
- WAL实例级别的互斥锁
- 写入操作的互斥访问
- 恢复过程的互斥访问

### 5.2 线程安全性
- 所有公共接口都是线程安全的
- 避免死锁和竞态条件

## 6. 性能优化

### 6.1 写入优化
- 批量写入
- 异步刷盘选项
- 预分配文件空间

### 6.2 恢复优化
- 并行恢复多个段
- 跳过无效记录
- 内存预分配

## 7. 调试和测试

### 7.1 日志记录
```c
ppdb_log_info("Writing WAL record: type=%d, key_size=%d, value_size=%d", 
              type, key_len, value_len);
```

### 7.2 测试用例
1. 基本功能测试
   - 写入和恢复
   - 删除操作
   - 错误处理

2. 并发测试
   - 多线程写入
   - 并发恢复

## 8. 常见问题和解决方案

### 8.1 删除操作处理
- 问题：删除操作可能失败，因为键不存在
- 解决：将"键不存在"视为删除成功

```c
if (err == PPDB_ERR_KEY_NOT_FOUND) {
    return PPDB_OK;  // 键不存在，视为删除成功
}
```

### 8.2 CRC校验失败
- 问题：CRC计算不一致
- 解决：使用统一的CRC32算法（crc32_z）

### 8.3 文件句柄泄漏
- 问题：异常情况下未关闭文件
- 解决：使用RAII模式和清理函数

## 9. 未来改进

1. 配置管理优化
   - 支持通过命令行参数配置WAL
   - 支持通过环境变量配置WAL
   - 支持通过配置文件（如YAML/JSON）配置WAL
   - 实现配置的动态重载
   - 添加配置验证和默认值处理

2. 压缩和归档
   - 实现日志压缩
   - 自动归档旧日志

3. 性能优化
   - 引入缓冲区
   - 实现批量写入
   - 优化CRC计算

4. 监控和统计
   - 添加性能指标
   - 实现状态监控

## 10. 配置管理计划

### 10.1 命令行参数

计划支持的命令行参数格式：
```bash
ppdb --wal-dir=/path/to/wal \
     --wal-segment-size=64MB \
     --wal-max-segments=10 \
     --wal-sync-write=true \
     --wal-use-buffer=true \
     --wal-buffer-size=4KB \
     --wal-compression=none
```

### 10.2 环境变量

计划支持的环境变量：
```bash
PPDB_WAL_DIR=/path/to/wal
PPDB_WAL_SEGMENT_SIZE=64MB
PPDB_WAL_MAX_SEGMENTS=10
PPDB_WAL_SYNC_WRITE=true
PPDB_WAL_USE_BUFFER=true
PPDB_WAL_BUFFER_SIZE=4KB
PPDB_WAL_COMPRESSION=none
```

### 10.3 配置文件

计划支持的YAML配置格式：
```yaml
wal:
  dir: /path/to/wal
  segment_size: 64MB
  max_segments: 10
  sync_write: true
  use_buffer: true
  buffer_size: 4KB
  compression: none
```

配置优先级（从高到低）：
1. 命令行参数
2. 环境变量
3. 配置文件
4. 默认值