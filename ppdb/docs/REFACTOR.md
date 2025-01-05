# PPDB 重构计划

## 架构分层

### 1. Core 层 (core.h/c)
- 系统基础设施
- 无业务逻辑
- 纯粹的基础功能

```c
// 示例接口
ppdb_error_t ppdb_core_init(void);
ppdb_error_t ppdb_core_shutdown(void);

// 同步原语
ppdb_error_t ppdb_core_mutex_create(ppdb_core_mutex_t** mutex);
ppdb_error_t ppdb_core_mutex_destroy(ppdb_core_mutex_t* mutex);

// 内存管理
void* ppdb_core_alloc(size_t size);
void ppdb_core_free(void* ptr);

// 文件系统
ppdb_error_t ppdb_core_file_open(const char* path, ppdb_core_file_t** file);
ppdb_error_t ppdb_core_file_close(ppdb_core_file_t* file);
```

### 2. Base 层 (base.h/c)
- 安全的基础抽象
- 资源自动管理
- 无指针暴露

```c
// 上下文管理
typedef uint64_t ppdb_context_t;
ppdb_error_t ppdb_context_create(ppdb_context_t* ctx);
void ppdb_context_destroy(ppdb_context_t ctx);

// 数据管理
typedef struct ppdb_data {
    uint8_t inline_data[32];
    uint32_t size;
    uint32_t flags;
} ppdb_data_t;

// 游标管理
typedef uint64_t ppdb_cursor_t;
ppdb_error_t ppdb_cursor_create(ppdb_context_t ctx, ppdb_cursor_t* cursor);
void ppdb_cursor_destroy(ppdb_cursor_t cursor);
```

### 3. Storage 层 (storage.h/c)
- 存储引擎实现
- 基于Base层接口
- 无需关心底层细节

```c
// 表操作
ppdb_error_t ppdb_table_create(ppdb_context_t ctx, const char* name);
ppdb_error_t ppdb_table_drop(ppdb_context_t ctx, const char* name);

// 数据操作
ppdb_error_t ppdb_put(ppdb_context_t ctx, ppdb_data_t key, ppdb_data_t value);
ppdb_error_t ppdb_get(ppdb_context_t ctx, ppdb_data_t key, ppdb_data_t* value);
ppdb_error_t ppdb_delete(ppdb_context_t ctx, ppdb_data_t key);

// 遍历操作
ppdb_error_t ppdb_scan(ppdb_context_t ctx, ppdb_cursor_t* cursor);
ppdb_error_t ppdb_next(ppdb_cursor_t cursor, ppdb_data_t* key, ppdb_data_t* value);
```

### 4. Peer 层 (peer.h/c)
- 网络和分布式功能
- 基于Storage层
- 处理远程交互

```c
// 网络服务
ppdb_error_t ppdb_server_start(const char* address, uint16_t port);
ppdb_error_t ppdb_server_stop(void);

// 客户端API
ppdb_error_t ppdb_client_connect(const char* address, uint16_t port);
ppdb_error_t ppdb_client_disconnect(void);

// 分布式协调
ppdb_error_t ppdb_cluster_join(const char* cluster_id);
ppdb_error_t ppdb_cluster_leave(void);
```

## 重构步骤

1. 创建新的目录结构
2. 实现Core层基础设施
3. 实现Base层安全抽象
4. 重构Storage层实现
5. 重构Peer层实现
6. 更新测试用例
7. 更新文档

## 注意事项

1. 保持向后兼容性
2. 确保线程安全
3. 优化性能
4. 完善错误处理
5. 添加详细注释
