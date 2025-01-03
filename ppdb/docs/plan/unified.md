# PPDB统一存储层次结构计划

## 1. 架构概览

### 1.1 层次结构
```
KVStore层 (kvstore_t)
    ↓
容器层 (container_t)
    ↓
存储层 (storage_t)
    ↓
基础层 (base_t)
```

### 1.2 类型体系
```c
// 统一的类型标记
typedef enum {
    PPDB_TYPE_SKIPLIST = 1,    // 基础跳表
    PPDB_TYPE_MEMTABLE = 2,    // 内存表
    PPDB_TYPE_SHARDED = 4,     // 分片表
    PPDB_TYPE_WAL = 8,         // 预写日志
    PPDB_TYPE_SSTABLE = 16     // 有序表
} ppdb_type_t;
```

## 2. 实现计划

### 2.1 基础层（base_t）实现
```c
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

#### 实施步骤
1. 基础结构实现（3天）
   - 统一的头部管理
   - 引用计数支持
   - 类型分发机制

2. 内存管理（4天）
   - 内存池实现
   - 对齐优化
   - 内存追踪

3. 性能优化（3天）
   - 缓存友好设计
   - 内联优化
   - 内存屏障优化

### 2.2 存储层（storage_t）实现
```c
typedef struct {
    ppdb_base_t base;         // 24字节
    ppdb_storage_ops_t* ops;  // 8字节
    ppdb_metrics_t metrics;   // 统计信息
} ppdb_storage_t;
```

#### 实施步骤
1. 基础接口实现（4天）
   - 读写接口
   - 同步接口
   - 统计接口

2. 具体实现（5天）
   - WAL实现
   - SSTable实现
   - 缓存实现

3. 性能优化（3天）
   - IO优化
   - 批量操作
   - 异步支持

### 2.3 容器层（container_t）实现
```c
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_container_ops_t* ops; // 8字节
    ppdb_storage_t* storage;   // 8字节
    ppdb_metrics_t metrics;    // 统计信息
} ppdb_container_t;
```

#### 实施步骤
1. 基础容器实现（5天）
   - Skiplist容器
   - Memtable容器
   - Sharded容器

2. 高级特性（4天）
   - 迭代器支持
   - 范围查询
   - 批量操作

3. 性能优化（3天）
   - 并发优化
   - 内存管理
   - 缓存优化

### 2.4 KVStore层（kvstore_t）实现
```c
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_kvstore_ops_t* ops;   // 8字节
    ppdb_container_t* active;  // 8字节
    ppdb_container_t* imm;     // 8字节
    ppdb_storage_t* wal;       // 8字节
    ppdb_storage_t** sst;      // 8字节
    ppdb_metrics_t metrics;    // 统计信息
} ppdb_kvstore_t;
```

#### 实施步骤
1. 基础功能实现（5天）
   - 事务支持
   - 快照实现
   - Compaction

2. 高级特性（4天）
   - 多版本支持
   - 增量压缩
   - 恢复机制

3. 性能优化（3天）
   - 写入优化
   - 读取优化
   - 资源管理

## 3. 测试计划

### 3.1 单元测试
- 每层基本功能测试
- 接口兼容性测试
- 错误处理测试

### 3.2 集成测试
- 层间交互测试
- 端到端功能测试
- 性能基准测试

### 3.3 压力测试
```c
// 性能指标
struct {
    // 基础指标
    uint64_t ops_per_sec;     // 操作数/秒
    uint64_t latency_us;      // 延迟(微秒)
    uint64_t throughput_mb;   // 吞吐量(MB/s)
    
    // 资源指标
    uint64_t memory_used;     // 内存使用
    uint64_t disk_used;       // 磁盘使用
    uint64_t cpu_usage;       // CPU使用率
} ppdb_perf_metrics_t;
```

## 4. 时间规划

### 第一阶段（3周）
- 基础层实现
- 存储层实现
- 基本测试框架

### 第二阶段（3周）
- 容器层实现
- KVStore层实现
- 集成测试

### 第三阶段（2周）
- 性能优化
- 压力测试
- 文档完善

## 5. 验收标准

### 5.1 功能指标
- 所有接口实现完整
- 测试覆盖率>90%
- 无内存泄漏
- 无数据损坏

### 5.2 性能指标
- 写入延迟<1ms (P99)
- 读取延迟<100us (P99)
- 单机吞吐>100K ops/s
- 内存使用可控

### 5.3 代码质量
- 接口文档完整
- 代码注释规范
- 错误处理完善
- 日志记录完整

## 6. 风险管理

### 6.1 技术风险
1. 接口设计不合理
2. 性能目标难达到
3. 内存管理复杂
4. 并发问题难调试

### 6.2 应对措施
1. 早期原型验证
2. 持续性能测试
3. 完善监控指标
4. 全面的测试用例
``` 