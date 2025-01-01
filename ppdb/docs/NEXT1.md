# 代码重构和抽象计划

## 模块层次关系

1. 最底层：skiplist
   - 基础的无锁数据结构
   - 提供基本的 KV 存储能力
   - 使用原子操作保证并发安全

2. 中间层：memtable
   - 基于 skiplist 实现
   - 添加了内存管理和生命周期控制
   - 提供更高级的接口（如迭代器）

3. 高级封装：sharded_memtable
   - 基于 memtable 实现
   - 添加了分片策略
   - 使用哈希函数进行分片

4. 独立组件：wal
   - 与存储引擎并行的持久化组件
   - 与 memtable 有交互但不是直接依赖关系
   - 也需要并发控制

## 潜在的可重用抽象

### 1. 引用计数管理（高度可重用）
```c
typedef struct {
    atomic_int ref_count;
    void* data;
    void (*cleanup)(void*);
} ppdb_refcounted_t;
```
- 实现代码完全可以重用
- 只需要实现不同的 cleanup 函数
- 计数器操作逻辑都是一样的

### 2. 迭代器接口（部分可重用）
```c
typedef struct {
    void* ctx;
    bool (*next)(void* ctx);
    bool (*valid)(void* ctx);
    void (*seek)(void* ctx, const void* key);
    void (*get)(void* ctx, void* key, void* value);
} ppdb_iterator_t;
```
- 接口定义可以重用
- 但每个模块需要自己实现具体函数
- 原因：
  - skiplist 需要处理节点遍历
  - memtable 需要处理版本和删除标记
  - wal 需要处理日志记录格式

### 3. 分片管理（高度可重用）
```c
typedef struct {
    uint32_t shard_bits;
    uint32_t shard_mask;
    uint32_t (*hash_func)(const void*, size_t);
    void** shards;
} ppdb_sharded_t;
```
- 分片计算逻辑可以完全重用
- 哈希函数可以共用
- 但初始化和清理需要模块自己实现
- 因为每个模块的分片内容不同

### 4. 并发控制工具（中度可重用）
```c
typedef struct {
    ppdb_sync_t* locks;
    uint32_t lock_count;
    bool (*try_lock)(void* ctx, uint32_t idx);
    void (*unlock)(void* ctx, uint32_t idx);
} ppdb_concurrent_t;
```
- 基本的锁操作可以重用
- 但具体的锁策略需要模块自己实现
- 原因：
  - sharded_memtable 需要考虑分片访问模式
  - wal 需要考虑日志追加特性
  - 不同模块的锁粒度要求不同

### 5. 内存池管理（低度可重用）
```c
typedef struct {
    size_t block_size;
    atomic_size_t used_size;
    atomic_size_t total_size;
    ppdb_sync_t lock;
    void* free_list;
} ppdb_mempool_t;
```
- 基本的分配/释放接口可以重用
- 但每个模块可能需要自己的策略：
  - skiplist 需要固定大小的节点分配
  - memtable 需要变长的 key-value 存储
  - wal 需要大块的顺序写入缓冲

## 建议实现顺序

1. 完全可重用的部分：
   - 引用计数管理
   - 分片计算
   - 哈希函数

2. 部分可重用的部分：
   - 并发控制的基本操作（提供默认实现）
   - 内存池的基本接口（提供默认实现）

3. 接口统一的部分：
   - 迭代器接口（定义接口规范）
   - 内存管理策略（定义接口规范）

## 目录结构建议

```
src/common/
  ├── refcount.h/c    - 引用计数
  ├── iterator.h/c    - 迭代器接口
  ├── sharding.h/c    - 分片管理
  ├── concurrent.h/c  - 并发控制
  └── mempool.h/c     - 内存池
```

## 预期收益

1. 减少重复代码
2. 统一接口设计
3. 方便测试和维护
4. 提高代码复用性
5. 降低模块间耦合

## 其他可能的抽象优化

除了上述基础组件的抽象，还发现以下几个可以优化的方向：

### 1. 错误处理机制
```c
typedef struct {
    int code;
    const char* message;
    const char* file;
    int line;
    void* context;
} ppdb_error_context_t;
```
- 统一错误处理和日志记录
- 所有模块都在重复类似的错误处理代码
- 可以提供宏来简化错误处理

### 2. 配置管理
```c
typedef struct {
    const char* name;
    const char* type;
    void* value;
    size_t size;
} ppdb_config_item_t;
```
- 目前配置分散在各个模块
- 可以统一配置的加载和验证
- 支持配置的热重载

### 3. 监控和指标
```c
typedef struct {
    const char* name;
    ppdb_metric_type_t type;
    atomic_uint64_t value;
    void (*update)(void* ctx, uint64_t val);
} ppdb_metric_t;
```
- 统一指标收集机制
- 提供统一的监控接口
- 支持自定义指标

### 4. 资源管理
```c
typedef struct {
    void* handle;
    size_t size;
    void (*cleanup)(void*);
    bool (*is_valid)(void*);
} ppdb_resource_t;
```
- 统一资源的生命周期管理
- 提供资源池和缓存机制
- 简化资源清理逻辑

### 5. 日志接口
```c
typedef struct {
    const char* module;
    int level;
    void (*write)(const char* fmt, ...);
    void (*flush)(void);
} ppdb_logger_t;
```
- 统一日志格式和接口
- 支持按模块和级别过滤
- 提供异步日志功能

### 6. 文件操作
```c
typedef struct {
    int fd;
    char* path;
    size_t size;
    bool is_temp;
    void (*on_error)(void* ctx);
} ppdb_file_t;
```
- 统一文件操作接口
- 提供缓存和预读机制
- 简化错误处理

### 7. 类型系统优化
```c
typedef struct {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    uint64_t version;
} ppdb_kv_t;
```
- 减少类型转换
- 统一内存布局
- 简化序列化逻辑

## 优化建议

### 实现优先级
1. 错误处理和日志接口（影响最广）
2. 资源管理（基础设施）
3. 类型系统（接口简化）
4. 配置管理（易于实现）
5. 监控和指标（可以渐进改进）
6. 文件操作（可以分阶段实现）

### 预期收益
1. 代码量减少 20-30%
2. 错误处理更统一
3. 接口更清晰
4. 维护成本降低
5. 测试更容易

### 建议目录结构
```
src/common/
  ├── error/          - 错误处理
  ├── config/         - 配置管理
  ├── metrics/        - 监控和指标
  ├── resource/       - 资源管理
  ├── log/           - 日志系统
  ├── fs/            - 文件操作
  └── types/         - 基础类型定义
``` 