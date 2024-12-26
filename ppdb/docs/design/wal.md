# WAL (Write-Ahead Logging) 实现文档

## 1. 概述

WAL是PPDB中的一个关键组件，用于确保数据的持久性和一致性。它通过在执行实际的数据修改之前，先将修改记录写入日志文件，从而在系统崩溃时能够恢复数据。

### 1.1 设计目标

- 确保数据修改的持久性
- 支持崩溃恢复
- 跨平台兼容性
- 高性能和低延迟
- 线程安全

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

1. 压缩和归档
   - 实现日志压缩
   - 自动归档旧日志

2. 性能优化
   - 引入缓冲区
   - 实现批量写入
   - 优化CRC计算

3. 监控和统计
   - 添加性能指标
   - 实现状态监控 