# PPDB 优化设计 V4

## 1. 核心设计目标
1. 统一的类型系统
2. 高效的内存使用
3. 灵活的扩展性
4. 优秀的性能表现
5. 一致的错误处理
6. 完善的统计信息

## 2. 基础类型设计

### 2.1 错误处理
```c
// 统一的错误类型
typedef enum {
    PPDB_OK = 0,                    // 成功
    PPDB_ERR_INVALID_ARG = -1,      // 无效参数
    PPDB_ERR_OUT_OF_MEMORY = -2,    // 内存不足
    PPDB_ERR_NOT_FOUND = -3,        // 未找到
    PPDB_ERR_ALREADY_EXISTS = -4,   // 已存在
    PPDB_ERR_NOT_SUPPORTED = -5,    // 不支持
    PPDB_ERR_IO = -6,              // IO错误
    PPDB_ERR_CORRUPTED = -7,       // 数据损坏
    PPDB_ERR_INTERNAL = -8,        // 内部错误
} ppdb_error_t;
```

### 2.2 统计信息
```c
// 基础统计信息
typedef struct {
    atomic_uint64_t get_count;      // Get操作总数
    atomic_uint64_t get_hits;       // Get命中次数
    atomic_uint64_t put_count;      // Put操作次数
    atomic_uint64_t remove_count;   // Remove操作次数
    atomic_uint64_t total_keys;     // 总键数
    atomic_uint64_t total_bytes;    // 总字节数
    atomic_uint64_t cache_hits;     // 缓存命中
    atomic_uint64_t cache_misses;   // 缓存未命中
} ppdb_metrics_t;

// 存储统计信息
typedef struct {
    ppdb_metrics_t base_metrics;    // 基础统计
    size_t memory_used;            // 内存使用
    size_t memory_allocated;       // 内存分配
    size_t block_count;           // 块数量
} ppdb_storage_stats_t;
```

### 2.3 类型标记
```c
typedef enum {
    PPDB_TYPE_SKIPLIST = 1,    // 基础跳表
    PPDB_TYPE_MEMTABLE = 2,    // 内存表
    PPDB_TYPE_SHARDED = 4,     // 分片表
    PPDB_TYPE_WAL = 8,         // 预写日志
    PPDB_TYPE_SSTABLE = 16     // 有序表
} ppdb_type_t;
```

### 2.4 基础结构
```c
// 头部信息（4字节）
typedef struct {
    unsigned type : 4;        // 类型（16种）
    unsigned flags : 12;      // 状态标记
    unsigned refs : 16;       // 引用计数
} ppdb_header_t;

// 基础节点（24字节）
typedef struct {
    ppdb_header_t header;     // 4字节
    union {
        void* ptr;            // 通用指针
        uint64_t data;        // 内联数据
    };                        // 8字节
    void* extra;             // 额外数据，8字节
    uint32_t padding;        // 4字节(对齐)
} ppdb_node_t;

// 通用存储结构（24字节）
typedef struct {
    ppdb_header_t header;     // 4字节
    union {
        struct {
            union {
                void* head;    // skiplist
                int fd;        // wal/sst
            };
            union {
                void* pool;    // skiplist
                void* buffer;  // wal/cache
            };
        } storage;
        struct {
            size_t limit;      // memtable
            atomic_size_t used;
        } mem;
        struct {
            uint32_t count;
            void** ptrs;       // shards/sstables
        } array;
    };
} ppdb_base_t;
```

## 3. 分层架构设计

### 3.1 存储层
```c
// 存储层接口
typedef struct {
    ppdb_error_t (*write)(void* impl, const void* data, size_t size);
    ppdb_error_t (*read)(void* impl, void* buf, size_t size);
    ppdb_error_t (*sync)(void* impl);
    ppdb_error_t (*get_stats)(void* impl, ppdb_storage_stats_t* stats);
} ppdb_storage_ops_t;

// 存储层实现
typedef struct {
    ppdb_base_t base;         // 24字节
    ppdb_storage_ops_t* ops;  // 8字节
    ppdb_metrics_t metrics;   // 统计信息
} ppdb_storage_t;            // 总大小：32字节 + metrics
```

### 3.2 容器层
```c
// 容器层接口
typedef struct {
    ppdb_error_t (*get)(void* impl, const ppdb_key_t* key, ppdb_value_t* value);
    ppdb_error_t (*put)(void* impl, const ppdb_key_t* key, const ppdb_value_t* value);
    ppdb_error_t (*remove)(void* impl, const ppdb_key_t* key);
    ppdb_error_t (*flush)(void* impl, ppdb_storage_t* dest);
} ppdb_container_ops_t;

// 容器层实现
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_container_ops_t* ops; // 8字节
    ppdb_storage_t* storage;   // 8字节
    ppdb_metrics_t metrics;    // 统计信息
} ppdb_container_t;           // 总大小：40字节 + metrics
```

### 3.3 KV存储层
```c
// KV存储层接口
typedef struct {
    ppdb_error_t (*begin_tx)(void* impl);
    ppdb_error_t (*commit_tx)(void* impl);
    ppdb_error_t (*snapshot)(void* impl, void** snap);
    ppdb_error_t (*compact)(void* impl);
    ppdb_error_t (*get_stats)(void* impl, ppdb_storage_stats_t* stats);
} ppdb_kvstore_ops_t;

// KV存储层实现
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_kvstore_ops_t* ops;   // 8字节
    ppdb_container_t* active;  // 8字节
    ppdb_container_t* imm;     // 8字节
    ppdb_storage_t* wal;       // 8字节
    ppdb_storage_t** sst;      // 8字节
    ppdb_metrics_t metrics;    // 统计信息
} ppdb_kvstore_t;             // 总大小：64字节 + metrics
```

## 4. 优化实现

### 4.1 错误处理优化
```c
// 错误处理宏
#define PPDB_TRY(expr) do { \
    ppdb_error_t err = (expr); \
    if (err != PPDB_OK) return err; \
} while (0)

// 错误信息获取
const char* ppdb_error_string(ppdb_error_t err);

// 错误转换
ppdb_error_t ppdb_system_error(void);
```

### 4.2 统计信息优化
```c
// 原子更新宏
#define PPDB_METRIC_INC(metric) \
    atomic_fetch_add(&(metric), 1)

#define PPDB_METRIC_ADD(metric, val) \
    atomic_fetch_add(&(metric), (val))

// 统计信息合并
void ppdb_metrics_merge(ppdb_metrics_t* dst, const ppdb_metrics_t* src);

// 统计信息快照
void ppdb_metrics_snapshot(const ppdb_metrics_t* metrics, ppdb_metrics_t* snapshot);
```

### 4.3 内存优化
1. 内联关键字段
2. 延迟分配
3. 字段复用

### 4.4 性能优化
1. 快速路径优化
2. 并发优化
3. IO优化

## 5. 扩展性设计

### 5.1 插件系统
```c
typedef struct {
    ppdb_type_t type;
    const char* name;
    void* plugin_data;
    ppdb_storage_ops_t storage_ops;
    ppdb_container_ops_t container_ops;
    ppdb_kvstore_ops_t kvstore_ops;
} ppdb_plugin_t;
```

### 5.2 监控和分析
- 完整的统计信息
- 性能分析支持
- 监控接口

## 6. 最新设计决策

1. 错误处理统一
   - 使用 ppdb_error_t 替代 ppdb_status_t
   - 统一错误码命名规范
   - 添加错误信息转换功能

2. 统计信息增强
   - 使用原子操作保证并发安全
   - 分层统计信息收集
   - 支持统计信息快照

3. 接口简化
   - 移除冗余接口
   - 统一接口命名
   - 简化参数传递

4. 性能优化
   - 批量操作支持
   - 异步操作接口
   - 缓存优化

5. 可维护性提升
   - 完善文档
   - 统一代码风格
   - 增加测试覆盖
