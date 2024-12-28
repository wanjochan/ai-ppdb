# PPDB 无锁版本设计文档

## 1. 设计目标

### 1.1 功能目标
- 提供与有锁版本相同的功能接口
- 支持完全无锁的并发操作
- 保证线程安全和数据一致性
- 支持高性能的读写操作

### 1.2 性能目标
- 单节点写入性能 > 100K QPS
- 单节点读取性能 > 500K QPS
- P99延迟 < 1ms
- 线性扩展能力

## 2. 核心组件设计

### 2.1 无锁跳表
```c
// 节点状态
typedef enum {
    NODE_VALID = 0,      // 正常节点
    NODE_MARKED = 1,     // 已标记删除
    NODE_INSERTING = 2,  // 正在插入
    NODE_HELPING = 3     // 正在帮助其他操作
} node_state_t;

// 节点结构
struct atomic_node {
    ppdb_slice_t key;                    // 键
    ppdb_slice_t value;                  // 值
    int height;                          // 节点高度
    _Atomic node_state_t state;          // 节点状态
    _Atomic(atomic_node_t*) next[0];     // 后继节点数组
};
```

#### 状态转换
1. VALID -> MARKED：标记删除
2. VALID -> INSERTING：开始插入
3. INSERTING -> VALID：插入完成
4. INSERTING -> HELPING：其他线程帮助完成插入
5. HELPING -> VALID：帮助操作完成

#### 内存管理
- 使用引用计数进行内存管理
- 延迟删除机制避免ABA问题
- 内存池减少内存分配开销

### 2.2 分片内存表
```c
// 分片配置
typedef struct {
    size_t shard_count;        // 分片数量
    size_t shard_size;         // 每个分片大小
    bool auto_resize;          // 自动调整大小
} shard_config_t;

// 分片表
struct sharded_memtable {
    atomic_skiplist_t** shards;         // 分片数组
    _Atomic size_t total_size;          // 总大小
    shard_config_t config;              // 配置
};
```

#### 分片策略
1. 静态分片：固定分片数量
2. 动态分片：根据负载自动调整
3. 一致性哈希：支持动态伸缩

### 2.3 无锁WAL
```c
// WAL记录类型
typedef enum {
    WAL_PUT = 1,
    WAL_DELETE = 2,
    WAL_CHECKPOINT = 3
} wal_record_type_t;

// WAL记录
struct wal_record {
    uint64_t sequence;                  // 序列号
    wal_record_type_t type;            // 记录类型
    ppdb_slice_t key;                  // 键
    ppdb_slice_t value;                // 值（可选）
};
```

#### 写入机制
1. 批量写入缓冲
2. 无锁队列实现
3. 异步刷盘策略

## 3. 并发控制

### 3.1 原子操作
- 使用C11原子操作
- 显式内存序控制
- 避免伪共享

### 3.2 一致性保证
- 线性一致性读写
- 快照隔离支持
- 原子批量操作

### 3.3 死锁避免
- 无锁算法设计
- 帮助机制实现
- 超时和回退策略

## 4. 性能优化

### 4.1 内存管理
- 自定义内存分配器
- 内存池实现
- 垃圾回收机制

### 4.2 缓存优化
- 缓存行对齐
- 预取机制
- 数据局部性优化

### 4.3 并发优化
- 细粒度并发
- 批量操作支持
- 无锁数据结构

## 5. 监控和诊断

### 5.1 性能指标
- 操作延迟统计
- 吞吐量监控
- 内存使用跟踪
- 并发度量度

### 5.2 调试支持
- 详细日志记录
- 状态跟踪
- 异常诊断
- 性能分析

## 6. 测试策略

### 6.1 功能测试
- 单元测试
- 集成测试
- 并发测试
- 故障注入

### 6.2 性能测试
- 基准测试
- 压力测试
- 长稳测试
- 极限测试

## 7. 注意事项

### 7.1 内存管理
- 确保正确的内存释放
- 避免内存泄漏
- 处理内存耗尽情况

### 7.2 并发安全
- 保证操作原子性
- 处理ABA问题
- 避免死锁和活锁

### 7.3 错误处理
- 优雅降级
- 错误恢复
- 异常处理 