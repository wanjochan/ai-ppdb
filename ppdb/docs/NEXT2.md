# 代码重构和抽象计划

## 系统架构概览

### 1. 项目结构
```
PPDB 项目
├── memkv    - 纯内存 KV 存储系统
│   特点：高性能、内存存储、支持过期、协议兼容
│
└── diskv    - 持久化 KV 存储系统
    特点：持久化、事务支持、一致性保证
```

### 2. 分层结构
```
应用层（API/CLI/Network Service）
    ↓
┌────────────┴───────────┐
↓                        ↓
memkv                   diskv
(内存KV存储)             (持久化KV存储)
    ↓                        ↓
    └────────────┬─────────┘
                 ↓
            共享组件层
   - skiplist（有锁/无锁）
   - memtable
   - sync原语
   - 错误处理
   - 监控系统
```

### 3. 模块层次关系
```
最底层：skiplist（支持有锁/无锁两种实现）
    ↓
中间层：memtable（内存管理和生命周期）
    ↓
高级封装：sharded_memtable（分片策略）
    ↓
独立组件：wal（并行持久化）
```

## 可重用抽象分类

### 1. 高度可重用组件

#### Skiplist 实现
```c
// skiplist 配置结构
typedef struct {
    bool use_lockfree;          // 是否使用无锁版本
    size_t max_height;          // 最大层数
    size_t initial_capacity;    // 初始容量
    ppdb_metric_t* metrics;     // 性能指标收集
} ppdb_skiplist_config_t;

// 统一的性能指标结构
typedef struct {
    ppdb_metric_t put_latency;
    ppdb_metric_t get_latency;
    ppdb_metric_t delete_latency;
    ppdb_metric_t conflict_count;    // 并发冲突次数
    ppdb_metric_t retry_count;       // 重试次数（无锁版本特有）
    ppdb_metric_t lock_contention;   // 锁竞争（有锁版本特有）
} ppdb_skiplist_metrics_t;

// 统一的接口
typedef struct {
    void* impl;                 // 具体实现（有锁或无锁）
    ppdb_skiplist_config_t config;
    ppdb_skiplist_metrics_t* metrics;
} ppdb_skiplist_t;
```

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
- [ ] Skiplist 双实现支持
  - [ ] 统一接口设计
  - [ ] 有锁版本完善
  - [ ] 无锁版本实现
  - [ ] 性能指标收集
  - [ ] 完整测试覆盖
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

## Skiplist 实现规划

### 1. 代码组织
```
src/kvstore/skiplist/
  ├── interface.h     - 统一接口定义
  ├── common.h        - 共享类型和常量
  ├── metrics.h       - 性能指标定义
  ├── locked.c        - 有锁版本实现
  ├── lockfree.c      - 无锁版本实现
  └── utils.c         - 共享工具函数
```

### 2. 测试覆盖
```
test/white/skiplist/
  ├── test_basic.c    - 基础功能测试
  ├── test_concurrent.c- 并发测试
  ├── test_edge.c     - 边界条件测试
  ├── test_iterator.c - 迭代器测试
  └── benchmark/      - 性能测试
      ├── single_thread.c  - 单线程基准
      ├── multi_thread.c   - 多线程扩展性
      └── mixed_ops.c      - 混合操作测试
```

### 3. 性能测试矩阵
- 数据规模：小(1K)、中(100K)、大(1M)
- 线程数：1、2、4、8、16、32
- 操作比例：
  * 读多写少 (90/10)
  * 读写均衡 (50/50)
  * 写多读少 (10/90)
- 数据分布：
  * 顺序
  * 随机
  * 热点

### 4. 监控指标
- 基础指标：
  * 操作延迟（avg/p99）
  * 吞吐量
  * 内存使用
- 版本特有指标：
  * 有锁版本：锁竞争、等待时间
  * 无锁版本：CAS 冲突、重试次数

### 5. 配置选项
- 版本选择：
  * 环境变量：PPDB_SKIPLIST_TYPE
  * 配置文件：skiplist_type
- 性能参数：
  * 初始大小
  * 最大层数
  * 内存限制
  * 并发参数

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
5. 性能测试报告
6. 版本切换工具

## memkv 实现规划

### 1. 核心组件
```c
// 网络服务
typedef struct {
    int port;
    void* event_loop;
    ppdb_metric_t* metrics;
    void (*on_request)(void* ctx, const char* cmd, void* args);
} ppdb_server_t;

// 协议解析
typedef struct {
    char* command;
    char* key;
    void* data;
    size_t data_len;
    uint32_t flags;
    time_t exptime;
} ppdb_protocol_request_t;

// 过期管理
typedef struct {
    uint64_t expire_time;
    void* data;
    size_t data_len;
    uint32_t flags;
} ppdb_cache_item_t;
```

### 2. 目录结构
```
src/
  ├── memkv/
  │   ├── server.c     - 网络服务
  │   ├── protocol.c   - 协议处理
  │   └── expire.c     - 过期管理
  ├── kvstore/         - 共享组件
  │   ├── skiplist/    
  │   └── memtable/    
  └── common/          - 基础设施
      └── sync/        

test/
  ├── memkv/          
  │   ├── test_protocol.c
  │   ├── test_expire.c
  │   └── benchmark/
  │       ├── single_client.c
  │       └── multi_clients.c
  └── kvstore/         - 共享组件测试
```

### 3. 接口设计
```c
// 基本接口
ppdb_error_t ppdb_cache_set(const char* key, const void* value, 
    size_t value_len, uint32_t flags, time_t exptime);
ppdb_error_t ppdb_cache_get(const char* key, void** value, 
    size_t* value_len, uint32_t* flags);
ppdb_error_t ppdb_cache_delete(const char* key);
ppdb_error_t ppdb_cache_incr(const char* key, uint64_t delta, 
    uint64_t* new_value);
ppdb_error_t ppdb_cache_decr(const char* key, uint64_t delta, 
    uint64_t* new_value);

// 统计接口
ppdb_error_t ppdb_cache_stats(ppdb_stats_t* stats);
```

### 4. 配置选项
```c
typedef struct {
    uint16_t port;              // 服务端口
    size_t max_memory;          // 最大内存使用
    size_t max_item_size;       // 单个项最大大小
    bool use_lockfree;          // 是否使用无锁实现
    int worker_threads;         // 工作线程数
} ppdb_cache_config_t;
```