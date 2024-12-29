# PPDB 无锁实现设计

> 本文档详细说明了 PPDB 的无锁数据结构设计和实现。它是性能优化的关键组件，负责提供高并发访问能力。相关文档：
> - MemTable 设计见 `design/memtable.md`
> - 整体设计见 `overview/DESIGN.md`
> - 开发规范见 `overview/DEVELOPMENT.md`

## 实现状态
✅ 已完成基本实现
- [x] 统一同步原语 (sync)
  - [x] 互斥锁模式
  - [x] 无锁模式
  - [x] 分片锁优化
  - [x] 引用计数管理
- [x] 无锁跳表（atomic_skiplist）
  - [x] 原子操作支持
  - [x] 引用计数内存管理
  - [x] 无锁并发操作
- [x] 分片内存表（sharded_memtable）
  - [x] 基于无锁跳表
  - [x] 分片并发优化
  - [x] 原子计数器

## 1. 设计目标

### 1.1 功能目标
- 提供统一的同步原语接口
- 支持互斥锁和无锁两种模式
- 保证线程安全和数据一致性
- 支持高性能的读写操作

### 1.2 性能目标
- 单节点写入性能 > 100K QPS
- 单节点读取性能 > 500K QPS
- P99延迟 < 1ms
- 线性扩展能力

## 2. 核心组件设计

### 2.1 同步原语
```c
// 节点状态
typedef enum ppdb_node_state {
    NODE_VALID = 0,      // 正常节点
    NODE_DELETED = 1,    // 已标记删除
    NODE_INSERTING = 2   // 正在插入
} ppdb_node_state_t;

// 引用计数
typedef struct ppdb_ref_count {
    atomic_uint count;   // 引用计数值
} ppdb_ref_count_t;

// 同步配置
typedef struct ppdb_sync_config {
    bool use_lockfree;        // 是否使用无锁模式
    uint32_t stripe_count;    // 分片锁数量
    uint32_t spin_count;      // 自旋次数
    uint32_t backoff_us;      // 退避时间
    bool enable_ref_count;    // 是否启用引用计数
} ppdb_sync_config_t;

// 同步原语
typedef struct ppdb_sync {
    union {
        atomic_int atomic;
        mutex_t mutex;
    } impl;
    ppdb_sync_config_t config;
    ppdb_ref_count_t* ref_count;
} ppdb_sync_t;
```

### 2.2 无锁跳表
```c
// 跳表节点
typedef struct skiplist_node {
    ppdb_sync_t sync;         // 同步原语
    void* key;                // 键
    uint32_t key_len;         // 键长度
    void* value;              // 值
    uint32_t value_len;       // 值长度
    uint32_t level;           // 节点层数
    struct skiplist_node* next[]; // 后继节点数组
} skiplist_node_t;
```

### 2.3 分片内存表
```c
// 分片配置
typedef struct {
    uint32_t shard_bits;       // 分片位数
    uint32_t shard_count;      // 分片数量
    uint32_t max_size;         // 每个分片的最大大小
} shard_config_t;

// 分片内存表
typedef struct {
    shard_config_t config;     // 分片配置
    ppdb_skiplist_t** shards;  // 分片数组
    ppdb_stripe_locks_t* locks; // 分片锁
    atomic_size_t total_size;  // 总元素个数
} sharded_memtable_t;
```

## 3. 实现细节

### 3.1 内存管理
- 使用引用计数进行内存管理
- 延迟删除避免ABA问题
- 原子操作保证线程安全

### 3.2 并发控制
- 统一的同步原语接口
- 支持互斥锁和无锁两种模式
- 分片锁减少竞争
- 自旋和退避策略优化

### 3.3 性能优化
- 分片数量优化为2的幂
- 缓存行对齐优化
- 内存布局优化
- 原子操作优化

## 4. 下一步计划
1. 实现无锁WAL
2. 优化分片策略
3. 添加性能测试
4. 进行压力测试
5. 补充单元测试

## 5. 监控和诊断

### 5.1 性能指标
- 竞争次数统计
- 等待时间统计
- 分片使用率统计

### 5.2 调试支持
- DEBUG模式下的详细统计
- 节点状态跟踪
- 引用计数监控

## 6. 注意事项

### 6.1 内存屏障
- 确保原子操作的正确性
- 避免编译器和CPU重排序
- 正确使用内存序

### 6.2 ABA问题
- 使用引用计数避免ABA
- 延迟删除节点
- 标记删除状态

### 6.3 性能调优
- 合理设置分片数量
- 优化自旋参数
- 调整退避策略