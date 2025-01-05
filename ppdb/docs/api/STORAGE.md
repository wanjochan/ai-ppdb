# PPDB 存储引擎 API

## 1. 概述

PPDB存储引擎提供了一套完整的C语言API，支持基本的键值操作和高级特性�?
## 2. 基础数据结构

### 2.1 数据库实�?```c
typedef struct ppdb_t ppdb_t;
```

### 2.2 配置选项
```c
typedef struct {
    const char* data_dir;     // 数据目录
    size_t cache_size;        // 缓存大小
    bool sync_write;          // 同步写入
    uint32_t max_file_size;   // 最大文件大�?    uint32_t block_size;      // 块大�?} ppdb_options_t;
```

### 2.3 迭代�?```c
typedef struct ppdb_iterator_t ppdb_iterator_t;
```

## 3. 基本操作API

### 3.1 实例管理
```c
// 创建实例
ppdb_error_t ppdb_open(const ppdb_options_t* options, ppdb_t** db);

// 关闭实例
void ppdb_close(ppdb_t* db);
```

### 3.2 读写操作
```c
// 写入数据
ppdb_error_t ppdb_put(ppdb_t* db, 
                      const uint8_t* key, size_t key_len,
                      const uint8_t* value, size_t value_len);

// 读取数据
ppdb_error_t ppdb_get(ppdb_t* db,
                      const uint8_t* key, size_t key_len,
                      uint8_t** value, size_t* value_len);

// 删除数据
ppdb_error_t ppdb_delete(ppdb_t* db,
                         const uint8_t* key, size_t key_len);
```

## 4. 高级特性API

### 4.1 批量操作
```c
// 开始批量操�?ppdb_error_t ppdb_batch_begin(ppdb_t* db);

// 提交批量操作
ppdb_error_t ppdb_batch_commit(ppdb_t* db);

// 回滚批量操作
ppdb_error_t ppdb_batch_rollback(ppdb_t* db);
```

### 4.2 迭代器操�?```c
// 创建迭代�?ppdb_error_t ppdb_iterator_create(ppdb_t* db,
                                 const uint8_t* start_key,
                                 size_t start_key_len,
                                 ppdb_iterator_t** iter);

// 移动到下一个键值对
ppdb_error_t ppdb_iterator_next(ppdb_iterator_t* iter);

// 获取当前键�?ppdb_error_t ppdb_iterator_get(ppdb_iterator_t* iter,
                              uint8_t** key, size_t* key_len,
                              uint8_t** value, size_t* value_len);

// 销毁迭代器
void ppdb_iterator_destroy(ppdb_iterator_t* iter);
```

## 5. 事务API

### 5.1 事务操作
```c
// 开始事�?ppdb_error_t ppdb_txn_begin(ppdb_t* db);

// 提交事务
ppdb_error_t ppdb_txn_commit(ppdb_t* db);

// 回滚事务
ppdb_error_t ppdb_txn_rollback(ppdb_t* db);
```

## 6. 快照API

### 6.1 快照管理
```c
// 创建快照
ppdb_error_t ppdb_snapshot_create(ppdb_t* db, 
                                 ppdb_snapshot_t** snapshot);

// 从快照读�?ppdb_error_t ppdb_snapshot_get(ppdb_snapshot_t* snapshot,
                              const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len);

// 释放快照
void ppdb_snapshot_release(ppdb_snapshot_t* snapshot);
```

## 7. 错误处理

### 7.1 错误�?```c
typedef enum {
    PPDB_OK = 0,
    PPDB_ERR_IO,
    PPDB_ERR_NOT_FOUND,
    PPDB_ERR_CORRUPTION,
    PPDB_ERR_NOT_SUPPORTED,
    PPDB_ERR_INVALID_ARGUMENT,
    PPDB_ERR_NO_MEMORY,
    PPDB_ERR_BATCH_TOO_LARGE,
    PPDB_ERR_TXN_CONFLICT
} ppdb_error_t;
```

### 7.2 错误信息
```c
// 获取错误描述
const char* ppdb_error_string(ppdb_error_t err);
```

## 8. 使用示例

### 8.1 基本操作示例
```c
ppdb_t* db;
ppdb_options_t options = {
    .data_dir = "/tmp/ppdb",
    .cache_size = 1024 * 1024,
    .sync_write = true
};

// 打开数据�?ppdb_error_t err = ppdb_open(&options, &db);
if (err != PPDB_OK) {
    fprintf(stderr, "Failed to open database: %s\n",
            ppdb_error_string(err));
    return 1;
}

// 写入数据
const char* key = "hello";
const char* value = "world";
err = ppdb_put(db, (uint8_t*)key, strlen(key),
               (uint8_t*)value, strlen(value));

// 关闭数据�?ppdb_close(db);
```
