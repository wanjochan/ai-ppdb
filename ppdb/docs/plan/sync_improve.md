# 同步原语层改进计划

## 背景

当前的同步原语实现较为基础,主要包含:
- 互斥锁(PPDB_SYNC_MUTEX)
- 自旋锁(PPDB_SYNC_SPINLOCK) 
- 读写锁(PPDB_SYNC_RWLOCK)

为了提升性能和可靠性,需要对同步原语层进行全面改进。

## 改进方案

### 1. 锁粒度优化

当前问题:
- memtable 和 skiplist 操作使用整表锁,粒度过大
- 在高并发场景下会造成严重的锁竞争

改进方案:
- 实现分片锁(sharding)机制
- 根据 key 进行哈希分片
- 每个分片使用独立的锁

示例实现:
```c
typedef struct ppdb_sharded_sync {
    ppdb_sync_t* locks;  // 锁数组
    uint32_t num_shards; // 分片数量
    uint32_t (*hash_func)(const void* key, size_t len); // 哈希函数
} ppdb_sharded_sync_t;
```

### 2. 读写锁优化

当前问题:
- 读写锁实现较为基础
- 缺乏场景适配能力
- 可能出现读写饥饿

改进方案:
- 增加读偏向(read-biased)模式
- 增加写偏向(write-biased)模式
- 增加公平模式
- 参考 Linux 内核的 brlock(big reader lock)实现

### 3. 无锁操作增强

当前问题:
- 无锁操作主要依赖 CAS
- 缺乏更高级的无锁机制

改进方案:
- 实现 RCU(Read-Copy-Update)机制
- 实现无锁队列
- 实现无锁哈希表

示例 RCU 实现:
```c
typedef struct ppdb_rcu_node {
    void* data;
    atomic_uint_least64_t version;
    struct ppdb_rcu_node* next;
} ppdb_rcu_node_t;
```

### 4. 性能监控与统计

当前问题:
- 缺乏详细的性能统计信息
- 难以发现性能瓶颈

改进方案:
- 增加锁等待时间分布统计
- 增加锁持有时间分布统计
- 增加锁竞争热点分析

示例统计结构:
```c
typedef struct ppdb_sync_metrics {
    atomic_uint_least64_t acquire_count;    // 获取次数
    atomic_uint_least64_t contention_count; // 竞争次数
    atomic_uint_least64_t wait_time_us;     // 等待时间(微秒)
    atomic_uint_least64_t hold_time_us;     // 持有时间(微秒) 
} ppdb_sync_metrics_t;
```

### 5. 死锁检测与预防

当前问题:
- 缺乏死锁检测机制
- 死锁问题难以排查

改进方案:
- 记录锁的获取顺序
- 检测循环依赖
- 实现超时检测

示例实现:
```c
typedef struct ppdb_deadlock_detector {
    ppdb_sync_t** locks;           // 已获取的锁
    uint32_t num_locks;            // 锁数量
    uint64_t acquire_timestamp;    // 获取时间戳
    uint32_t timeout_ms;          // 超时时间(毫秒)
} ppdb_deadlock_detector_t;
```

### 6. 自适应自旋

当前问题:
- 自旋策略较为简单
- 未考虑系统负载情况

改进方案:
- 根据历史竞争情况动态调整自旋次数
- 考虑 CPU 负载情况
- 考虑任务优先级

示例实现:
```c
typedef struct ppdb_adaptive_spin {
    atomic_uint spin_count;        // 当前自旋次数
    atomic_uint success_count;     // 成功次数
    atomic_uint failure_count;     // 失败次数
    uint32_t min_spins;           // 最小自旋次数
    uint32_t max_spins;           // 最大自旋次数
} ppdb_adaptive_spin_t;
```

### 7. 条件变量支持

当前问题:
- 缺乏条件变量支持
- 难以实现复杂的同步场景

改进方案:
- 增加条件变量支持
- 支持等待特定条件
- 支持生产者-消费者模式
- 支持任务同步

示例实现:
```c
typedef struct ppdb_sync_condition {
    ppdb_sync_t* mutex;           // 互斥锁
    pthread_cond_t cond;          // 条件变量
    bool (*predicate)(void*);     // 条件判断函数
    void* pred_arg;               // 条件判断参数
} ppdb_sync_condition_t;
```

## 实施计划

分四个阶段实施改进:

### 第一阶段 (2025 Q1)
- 实现分片锁机制
- 实现基本的性能监控

### 第二阶段 (2025 Q2)
- 增强无锁操作
- 优化读写锁实现

### 第三阶段 (2025 Q3)
- 添加死锁检测
- 实现条件变量支持

### 第四阶段 (2025 Q4)
- 实现自适应自旋
- 优化和完善各项特性

## 预期收益

1. 显著提升并发性能
2. 减少锁竞争
3. 提高系统可靠性
4. 增强可观测性
5. 支持更复杂的同步场景

## 风险评估

1. 代码复杂度增加
2. 可能引入新的并发 bug
3. 需要较多的测试工作
4. 性能优化可能带来内存开销

## 后续工作

1. 编写详细的设计文档
2. 开发测试用例
3. 进行性能基准测试
4. 收集实际使用数据
5. 持续优化和改进
