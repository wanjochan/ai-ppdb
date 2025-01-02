# PPDB 优化设计 V4

## 1. 核心设计目标
1. 统一的类型系统
2. 高效的内存使用
3. 灵活的扩展性
4. 优秀的性能表现

## 2. 基础类型设计

### 2.1 类型标记
```c
typedef enum {
    PPDB_TYPE_SKIPLIST = 1,    // 基础跳表
    PPDB_TYPE_MEMTABLE = 2,    // 内存表
    PPDB_TYPE_SHARDED = 4,     // 分片表
    PPDB_TYPE_WAL = 8,         // 预写日志
    PPDB_TYPE_SSTABLE = 16     // 有序表
} ppdb_type_t;
```

### 2.2 紧凑的基础结构
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
    int (*write)(void* impl, const void* data, size_t size);
    int (*read)(void* impl, void* buf, size_t size);
    int (*sync)(void* impl);
} ppdb_storage_ops_t;

// 存储层实现
typedef struct {
    ppdb_base_t base;         // 24字节
    ppdb_storage_ops_t* ops;  // 8字节
} ppdb_storage_t;            // 总大小：32字节
```

### 3.2 容器层
```c
// 容器层接口
typedef struct {
    int (*get)(void* impl, const ppdb_key_t* key, ppdb_value_t* value);
    int (*put)(void* impl, const ppdb_key_t* key, const ppdb_value_t* value);
    int (*flush)(void* impl, ppdb_storage_t* dest);
} ppdb_container_ops_t;

// 容器层实现
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_container_ops_t* ops; // 8字节
    ppdb_storage_t* storage;   // 8字节
} ppdb_container_t;           // 总大小：40字节
```

### 3.3 KV存储层
```c
// KV存储层接口
typedef struct {
    int (*begin_tx)(void* impl);
    int (*commit_tx)(void* impl);
    int (*snapshot)(void* impl, void** snap);
    int (*compact)(void* impl);
} ppdb_kvstore_ops_t;

// KV存储层实现
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_kvstore_ops_t* ops;   // 8字节
    ppdb_container_t* active;  // 8字节
    ppdb_container_t* imm;     // 8字节
    ppdb_storage_t* wal;       // 8字节
    ppdb_storage_t** sst;      // 8字节
} ppdb_kvstore_t;             // 总大小：64字节
```

## 4. 优化实现

### 4.1 类型分发优化
```c
// 常用类型快速路径
static inline ppdb_status_t ppdb_get(void* impl, const ppdb_key_t* key, ppdb_value_t* value) {
    ppdb_base_t* base = impl;
    
    // 常用类型用switch
    switch (base->header.type) {
    case PPDB_TYPE_SKIPLIST:
    case PPDB_TYPE_MEMTABLE:
        return fast_get_impl(impl, key, value);
    }
    
    // 不常用类型用函数表
    ppdb_get_fn get_fn = g_get_table[base->header.type];
    return get_fn ? get_fn(impl, key, value) : PPDB_INVALID_TYPE;
}
```

### 4.2 内存优化
1. 内联关键字段：
   - 类型信息（4字节）
   - 通用指针（8字节）
   - 额外数据（8字节）

2. 延迟分配：
   - 统计信息
   - 缓存数据
   - 扩展字段

3. 字段复用：
   - 使用union合并相似字段
   - 复用指针和整数字段
   - 对齐优化

### 4.3 性能优化
1. 快速路径：
   - 常用操作内联
   - 类型特定优化
   - 缓存友好设计

2. 并发优化：
   - 原子操作
   - 细粒度锁
   - 无锁数据结构

3. IO优化：
   - 批量操作
   - 异步IO
   - 预读/预写

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
```c
typedef struct {
    uint64_t ops_count;
    uint64_t bytes_written;
    uint64_t bytes_read;
    uint64_t cache_hits;
    uint64_t cache_misses;
} ppdb_metrics_t;
```

## 6. 内存使用总结

基础结构：
- ppdb_node_t: 24字节
- ppdb_base_t: 24字节
- ppdb_storage_t: 32字节
- ppdb_container_t: 40字节
- ppdb_kvstore_t: 64字节

优化效果：
1. 紧凑的基础结构（24字节）
2. 合理的内存对齐
3. 高效的字段复用
4. 按需的内存分配

## 7. 后续优化方向

1. 编译期优化：
   - 模板元编程
   - 内联优化
   - 常量折叠

2. 运行时优化：
   - JIT编译
   - 自适应优化
   - 动态特化

3. 内存优化：
   - 内存池
   - 对象缓存
   - 零拷贝

4. IO优化：
   - 多级缓存
   - 智能预取
   - 压缩优化

## 8. 文件变更计划

### 8.1 文件结构
```
新增文件：
include/ppdb/
  ├─ base.h          // 核心定义：类型系统、接口、数据结构 (~400行)
  └─ storage.h       // 存储接口：skiplist/memtable/sharded/kvstore (~300行)

src/
  ├─ base.c          // 基础功能实现 (~500行)
  └─ storage.c       // 所有存储实现 (~800行)

删除文件：
include/ppdb/
  ├─ skiplist.h      // 合并到 base.h 和 storage.h
  ├─ memtable.h      // 合并到 base.h 和 storage.h
  ├─ sharded.h       // 合并到 base.h 和 storage.h
  └─ kvstore.h       // 合并到 base.h 和 storage.h

src/
  ├─ skiplist.c      // 合并到 storage.c
  ├─ memtable.c      // 合并到 storage.c
  ├─ sharded.c       // 合并到 storage.c
  └─ kvstore.c       // 合并到 storage.c
```

### 8.2 代码分布

1. base.h (~400行):
   - 类型系统定义（type_t, header_t, node_t, base_t）
   - 统一接口定义（ops_t, stats_t）
   - 核心功能声明（init, get, put等）
   - 工具函数声明

2. storage.h (~300行):
   - 存储配置定义
   - 存储统计定义
   - 各类型特化接口
   - 工具函数声明

3. base.c (~500行):
   - 核心功能实现
   - 内存管理实现
   - 统计功能实现
   - 工具函数实现

4. storage.c (~800行):
   - skiplist 实现
   - memtable 实现
   - sharded 实现
   - kvstore 实现

### 8.3 优化机会

1. 代码重用：
   - 统一的类型系统减少重复定义
   - 共享的内存管理减少冗余代码
   - 统一的错误处理简化代码

2. 内联优化：
   - 关键路径函数内联
   - 小函数合并
   - 分支优化

3. 编译优化：
   - 减少头文件依赖
   - 优化包含关系
   - 减少编译时间

### 8.4 预期效果

1. 代码量变化：
   - 当前代码：~4000行
   - 优化后：~2000行
   - 减少约50%代码量

2. 维护性提升：
   - 文件数量从10个减少到4个
   - 接口更统一清晰
   - 实现更内聚

3. 性能提升：
   - 减少函数调用开销
   - 提高缓存命中率
   - 优化内存使用
