# 代码重构和抽象计划

## 系统架构概览

### 1. 分层结构
```
应用层（API/CLI）
    ↓
数据库层（PPDB）- 分布式协议、成员管理、一致性保证
    ↓
KV存储层（KVStore）- 键值操作、事务管理、版本控制
    ↓
存储层（Storage）- 内存表、WAL、持久化
    ↓
核心层（Core）- 同步原语、内存管理、错误处理
```

### 2. 模块层次关系
```
最底层：skiplist（无锁数据结构）
    ↓
中间层：memtable（内存管理和生命周期）
    ↓
高级封装：sharded_memtable（分片策略）
    ↓
独立组件：wal（并行持久化）
```

## 可重用抽象分类

### 1. 高度可重用组件

#### 引用计数管理
```c
typedef struct {
    atomic_int ref_count;
    void* data;
    void (*cleanup)(void*);
    ppdb_error_context_t* error_ctx;  // 新增：错误上下文
} ppdb_refcounted_t;
```

#### 分片管理
```c
typedef struct {
    uint32_t shard_bits;
    uint32_t shard_mask;
    uint32_t (*hash_func)(const void*, size_t);
    void** shards;
    ppdb_metric_t* metrics;  // 新增：性能指标
} ppdb_sharded_t;
```

### 2. 核心基础设施

#### 错误处理机制
```c
typedef struct {
    int code;
    const char* message;
    const char* file;
    int line;
    void* context;
    struct ppdb_error_context_t* parent;  // 新增：错误链
    ppdb_logger_t* logger;  // 新增：关联日志
} ppdb_error_context_t;
```

#### 资源管理
```c
typedef struct {
    void* handle;
    size_t size;
    void (*cleanup)(void*);
    bool (*is_valid)(void*);
    ppdb_metric_t* usage_metric;  // 新增：资源使用统计
    ppdb_error_context_t* error_ctx;  // 新增：错误上下文
} ppdb_resource_t;
```

#### 类型系统
```c
typedef struct {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    uint64_t version;
    uint32_t flags;  // 新增：扩展标志
    ppdb_resource_t* resource;  // 新增：资源管理
} ppdb_kv_t;
```

### 3. 运维支持组件

#### 配置管理
```c
typedef struct {
    const char* name;
    const char* type;
    void* value;
    size_t size;
    bool (*validate)(void* value);  // 新增：验证函数
    void (*on_change)(void* old_value, void* new_value);  // 新增：变更回调
} ppdb_config_item_t;
```

#### 监控和指标
```c
typedef struct {
    const char* name;
    ppdb_metric_type_t type;
    atomic_uint64_t value;
    void (*update)(void* ctx, uint64_t val);
    void (*collect)(void* ctx);  // 新增：指标收集
    ppdb_logger_t* logger;  // 新增：关联日志
} ppdb_metric_t;
```

#### 日志接口
```c
typedef struct {
    const char* module;
    int level;
    void (*write)(const char* fmt, ...);
    void (*flush)(void);
    bool (*should_log)(int level);  // 新增：日志级别控制
    ppdb_metric_t* metrics;  // 新增：日志指标
} ppdb_logger_t;
```

## 优化实施计划

### 1. 第一阶段：基础设施（当前优先级）
- [x] 错误处理框架
  - [x] 错误码统一
  - [x] 错误上下文
  - [ ] 错误链支持
  - [ ] 日志集成
- [ ] 资源管理框架
  - [ ] 基础资源跟踪
  - [ ] 内存限制
  - [ ] 资源清理
- [ ] 类型系统优化
  - [ ] 接口统一
  - [ ] 内存布局优化
  - [ ] 序列化支持

### 2. 第二阶段：核心组件
- [ ] 引用计数管理
  - [ ] 基础实现
  - [ ] 内存泄漏检测
  - [ ] 循环引用检测
- [ ] 分片管理
  - [ ] 哈希函数优化
  - [ ] 动态分片
  - [ ] 负载均衡

### 3. 第三阶段：运维支持
- [ ] 配置系统
  - [ ] 配置加载
  - [ ] 动态更新
  - [ ] 配置验证
- [ ] 监控系统
  - [ ] 指标收集
  - [ ] 性能分析
  - [ ] 告警机制
- [ ] 日志系统
  - [ ] 异步日志
  - [ ] 日志分级
  - [ ] 日志轮转

## 预期收益

1. 代码质量提升
   - 代码量减少 20-30%
   - 错误处理更统一
   - 接口更清晰
   - 维护成本降低

2. 性能优化
   - 内存使用更高效
   - 并发性能提升
   - IO 效率提高

3. 可维护性提升
   - 错误追踪更容易
   - 问题定位更快
   - 测试更方便

## 注意事项

1. 性能考虑
   - 避免关键路径开销
   - 优化内存分配
   - 保持数据局部性

2. 接口设计
   - 保持简单性
   - 避免过度抽象
   - 兼容性考虑

3. 实现要点
   - 平台兼容性
   - 并发安全性
   - 资源安全性

## 后续规划

1. 定期评估和调整
2. 收集性能数据
3. 及时响应反馈
4. 持续改进文档
