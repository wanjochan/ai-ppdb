# PPDB 架构设计

## 1. 总体架构

### 1.1 系统层次
```
应用层
  |
  |-- memkv               diskv
      |                    |
      |                    |
      |                   LSM存储引擎
      |                    |-- memtable
      |                    |-- wal
      |                    `-- sstable
      |                    
      `-----> skiplist <----´
             (基础组件)
             - 原生skiplist
             - 分片支持
             - 迭代器



基础层 (base.c)
├── 错误处理
├── 日志系统
├── 同步原语 (base_sync.inc.c)
├── 内存分配
└── 随机数生成

存储层 (storage.c)
├── 键值操作
├── 节点操作 (storage_misc.inc.c)
├── CRUD操作 (storage_crud.inc.c)
└── 迭代器操作 (storage_iterator.inc.c)


```

### 1.2 核心组件

#### skiplist（基础组件）
- 核心数据结构
- 支持并发访问
- 提供分片能力
- 实现迭代器接口

#### memkv（内存KV存储）
- 基于分片skiplist
- 高性能内存存储
- 支持过期时间
- 协议兼容层

#### diskv（持久化存储）
- 基于LSM树
- 使用skiplist实现memtable
- WAL保证持久性
- SSTable分层存储

## 2. 关键设计决策

### 2.1 统一的skiplist
- 作为共享基础组件
- 支持无锁/有锁两种模式
- 提供分片能力以支持并发
- 可由memkv和diskv复用

### 2.2 简化的层次结构
- 去除多余抽象层
- 直接使用具体实现
- 明确组件职责
- 降低维护成本

### 2.3 两条产品线
- memkv: 追求极致性能
- diskv: 保证数据持久性
- 共享基础设施
- 独立演进发展

## 3. 接口设计

### 3.1 skiplist接口
```c
typedef struct ppdb_skiplist {
    // 基础skiplist结构
    void* head;
    uint32_t level;
    size_t size;
    
    // 分片支持
    uint32_t shard_bits;
    uint32_t shard_mask;
    
    // 并发控制
    bool use_lock;
    ppdb_sync_t* locks;
    
    // 性能统计
    ppdb_metrics_t metrics;
} ppdb_skiplist_t;
```

### 3.2 memkv接口
```c
typedef struct ppdb_memkv {
    ppdb_skiplist_t* store;
    ppdb_config_t config;
    ppdb_metrics_t metrics;
} ppdb_memkv_t;
```

### 3.3 diskv接口
```c
typedef struct ppdb_diskv {
    // 内存部分
    ppdb_skiplist_t* active;
    ppdb_skiplist_t* imm;
    
    // 持久化部分
    ppdb_wal_t* wal;
    ppdb_sstable_t** sst;
    
    // 配置和统计
    ppdb_config_t config;
    ppdb_metrics_t metrics;
} ppdb_diskv_t;
```

## 4. 关键流程

### 4.1 写入流程
1. memkv
   - 直接写入分片skiplist
   - 更新统计信息

2. diskv
   - 写入WAL
   - 写入active memtable
   - 必要时触发compaction

### 4.2 读取流程
1. memkv
   - 从分片skiplist读取
   - 检查过期时间

2. diskv
   - 查找active memtable
   - 查找immutable memtable
   - 按层查找SSTable

### 4.3 compaction流程（diskv）
1. 触发条件
   - memtable达到阈值
   - 手动触发
   - 后台定时触发

2. 处理步骤
   - 冻结当前memtable
   - 创建新的memtable
   - 后台进行合并
   - 更新元数据

## 5. 监控指标

### 5.1 skiplist指标
- 节点数量
- 内存使用
- 操作延迟
- 分片负载

### 5.2 memkv指标
- QPS统计
- 延迟分布
- 内存使用
- 过期统计

### 5.3 diskv指标
- 写放大
- 读放大
- 空间放大
- compaction统计

## 6. 后续演进

### 6.1 近期规划
1. skiplist增强
   - 完善分片机制
   - 优化并发控制
   - 提升遍历性能

2. memkv增强
   - 支持更多协议
   - 增加集群功能
   - 优化过期机制

3. diskv增强
   - 优化compaction
   - 改进缓存策略
   - 增加事务支持

### 6.2 长期规划
1. 分布式支持
2. 更多存储引擎
3. 云原生适配
4. 监控体系完善 

