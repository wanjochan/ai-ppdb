# PPDB 后续优化计划

## MemTable 优化

### 1. 细粒度锁方案

#### 1.1 改动范围
```
ppdb/include/ppdb/
  ├── memtable.h      // 接口定义，添加锁表结构
  ├── skiplist.h      // 跳表接口修改
  └── atomic.h        // 原子操作封装

ppdb/src/kvstore/
  ├── memtable.c      // 实现细粒度锁逻辑
  └── skiplist.c      // 修改并发控制
```

#### 1.2 核心结构
```c
// 锁表结构
struct ppdb_lock_table_t {
    rwlock_t* locks;           // 锁数组
    uint32_t mask;            // 快速取模用
    uint32_t size;            // 锁数量，建议为 CPU 核心数的 2-4 倍
};

// 锁操作封装
static inline uint32_t get_lock_index(const void* key, size_t len) {
    uint32_t hash = ppdb_hash(key, len);
    return hash & lock_table->mask;
}
```

#### 1.3 改动规模
- 新增代码：~250 行
- 修改代码：~150 行
- 测试代码：~100 行
- 总计：~500 行

#### 1.4 实施步骤
1. 实现基础锁框架
2. 添加配置开关（默认关闭）
3. 逐个接口迁移
4. 并发测试
5. 性能对比和调优
6. 稳定后默认启用

### 2. 其他性能优化方向

#### 2.1 分层设计（预计提升 20-30%）
- 热数据层：紧凑存储，减少指针跳转
- 温数据层：批量压缩，预排序优化

#### 2.2 内存管理优化（预计提升 10-15%）
```c
struct mem_pool_t {
    void* blocks[MAX_BLOCKS];    // 固定大小块
    size_t block_size;           // 典型值 4KB
    atomic_int free_count;       // 可用块计数
    atomic_int next_free;        // 下一个可用位置
}
```

#### 2.3 SIMD 加速（预计提升 30-40%）
- 键比较操作优化
- 批量数据处理
- 向量化操作

#### 2.4 查找优化（预计提升 15-20%）
```c
struct skip_hint_t {
    atomic_uint_least64_t last_pos;   // 上次位置
    atomic_uint_least32_t height;     // 建议高度
    char prefix[8];                   // 前缀缓存
}
```

#### 2.5 压缩优化（预计节省 30-50% 内存）
```c
struct compressed_node_t {
    uint16_t key_prefix_len;          // 前缀长度
    uint16_t key_suffix_len;          // 后缀长度
    uint32_t value_len;               // 值长度
    char data[];                      // 压缩数据
}
```

## WAL 性能优化

### 1. Group Commit（预计提升 30-40%）

#### 1.1 核心结构
```c
struct wal_group_t {
    struct {
        void* data;                  // 数据指针
        size_t size;                 // 数据大小
        ppdb_callback_t callback;    // 完成回调
    } entries[WAL_GROUP_SIZE];
    
    atomic_size_t count;             // 当前条目数
    atomic_bool committing;          // 提交状态
    condition_t not_full;            // 未满条件变量
    condition_t committed;           // 已提交条件变量
};
```

#### 1.2 实现策略
- 批量写入阈值：4KB 或 16 条记录
- 超时时间：2ms
- 动态调整：根据负载自适应

### 2. WAL 缓冲池（预计提升 20-30%）

#### 2.1 设计结构
```c
struct wal_buffer_pool_t {
    struct buffer_block {
        char data[WAL_BLOCK_SIZE];   // 典型值 64KB
        size_t used;                 // 已使用大小
        atomic_bool in_use;          // 使用状态
    } blocks[WAL_POOL_SIZE];
    
    atomic_uint next_write;          // 写入位置
    atomic_uint next_flush;          // 刷盘位置
};
```

#### 2.2 关键策略
- 双缓冲切换
- 预分配内存
- 零拷贝写入

### 3. 异步预写（预计提升 25-35%）

#### 3.1 实现结构
```c
struct async_wal_t {
    ring_buffer_t* pending;          // 等待队列
    thread_pool_t* workers;          // 工作线程池
    atomic_bool background_flush;     // 后台刷盘状态
    
    struct {
        atomic_uint_least64_t total_writes;    // 总写入数
        atomic_uint_least64_t group_commits;   // 组提交数
        atomic_uint_least64_t buffer_hits;     // 缓冲命中数
    } stats;
};
```

#### 3.2 优化策略
- 预测性写入
- 智能批处理
- 自适应刷盘

### 4. CRC 优化（预计提升 10-15%）

#### 4.1 硬件加速
```c
// 硬件 CRC32-C 支持
#if defined(__x86_64__) || defined(_M_X64)
    #define HARDWARE_CRC32C
    static inline uint32_t hw_crc32c(uint32_t crc, const void* data, size_t len) {
        const uint8_t* p = data;
        while (len >= sizeof(uint64_t)) {
            crc = _mm_crc32_u64(crc, *(uint64_t*)p);
            p += sizeof(uint64_t);
            len -= sizeof(uint64_t);
        }
        // 处理剩余字节...
        return crc;
    }
#endif
```

#### 4.2 优化方案
- 分片并行计算
- 增量式更新
- 查表优化

### 5. 文件 IO 优化（预计提升 15-25%）

#### 5.1 核心结构
```c
struct wal_io_manager_t {
    int fd;                          // 文件描述符
    void* mapped_addr;               // 内存映射地址
    size_t mapped_size;              // 映射大小
    atomic_size_t write_pos;         // 写入位置
    
    struct {
        bool use_direct_io;          // 直接 IO
        bool use_mmap;               // 内存映射
        size_t write_buffer_size;    // 写缓冲大小
        int fdatasync_interval;      // 同步间隔
    } options;
};
```

#### 5.2 优化策略
- 直接 IO（绕过系统缓存）
- 内存映射（大文件）
- 对齐写入
- 预分配空间

### 6. 实施优先级

1. Group Commit
   - 收益最大
   - 实现相对简单
   - 风险可控

2. WAL 缓冲池
   - 基础设施改进
   - 为其他优化打基础
   - 改动相对独立

3. 异步预写
   - 较大收益
   - 需要仔细处理并发
   - 依赖缓冲池

4. IO 优化
   - 平台相关性强
   - 需要充分测试
   - 建议渐进式改进

5. CRC 优化
   - 收益稳定
   - 改动范围小
   - 可选优化

### 7. 注意事项

1. 正确性保证
   - 严格的故障恢复测试
   - 并发正确性验证
   - 压力测试

2. 监控指标
   - 延迟分布
   - 吞吐量
   - 磁盘使用
   - 内存占用

3. 降级策略
   - 配置开关
   - 运行时调整
   - 故障回退

## 优先级建议

1. 细粒度锁方案（已有无锁分支，可对比）
2. SIMD 加速（收益显著，风险可控）
3. 分层设计（架构调整，需要仔细设计）
4. 内存管理优化（基础设施改进）
5. 查找优化（增量改进）
6. 压缩优化（可选优化）
