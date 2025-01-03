# 统一存储层计划

## 1. 总体目标

基于skiplist实现memkv和diskv两条产品线，提供高性能的内存KV存储和持久化KV存储功能。

## 2. 架构设计

### 2.1 核心组件

```c
// 基础skiplist
typedef struct ppdb_skiplist {
    void* head;
    uint32_t level;
    size_t size;
    ppdb_metrics_t metrics;
} ppdb_skiplist_t;

// memkv产品线
typedef struct ppdb_memkv {
    ppdb_skiplist_t* store;
    ppdb_config_t config;
    ppdb_metrics_t metrics;
} ppdb_memkv_t;

// diskv产品线
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

### 2.2 组件关系

1. skiplist作为基础组件
   - 提供基本KV操作
   - 支持并发访问
   - 提供迭代器接口

2. memkv基于skiplist
   - 纯内存KV存储
   - 支持过期时间
   - 高性能读写

3. diskv基于skiplist
   - LSM树结构
   - WAL日志
   - SSTable持久化

## 3. 实现计划

### 3.1 skiplist实现

1. 基础结构
   - 节点设计
   - 层级管理
   - 内存分配

2. 基本操作
   - 插入/删除
   - 查找/更新
   - 范围扫描

3. 并发控制
   - 分片锁
   - 无锁操作
   - 引用计数

### 3.2 memkv实现

1. 基础功能
   - KV接口
   - 过期管理
   - 统计信息

2. 高级特性
   - 批量操作
   - 原子操作
   - 事务支持

3. 性能优化
   - 内存管理
   - 并发优化
   - 缓存优化

### 3.3 diskv实现

1. 内存管理
   - active/imm管理
   - 内存限制
   - flush策略

2. 持久化
   - WAL实现
   - SSTable格式
   - 压缩编码

3. 后台任务
   - compaction
   - 垃圾回收
   - 检查点

## 4. 测试计划

### 4.1 功能测试
- 基本操作测试
- 并发正确性
- 错误处理

### 4.2 性能测试
- 延迟指标
- 吞吐量测试
- 资源占用

### 4.3 稳定性测试
- 压力测试
- 故障恢复
- 长稳测试

## 5. 时间规划

### 5.1 第一阶段（4周）
1. 第1-2周：skiplist基础功能
2. 第3-4周：skiplist并发控制

### 5.2 第二阶段（3周）
1. 第5周：memkv基础功能
2. 第6-7周：memkv高级特性

### 5.3 第三阶段（3周）
1. 第8周：diskv内存管理
2. 第9-10周：diskv持久化

## 6. 验收标准

### 6.1 功能标准
- 接口完整性
- 并发正确性
- 持久化可靠性

### 6.2 性能标准
- 读写延迟
- 内存占用
- 磁盘IO

### 6.3 代码质量
- 测试覆盖
- 文档完善
- 代码规范

## 7. 风险管理

### 7.1 技术风险
1. 性能问题
   - 跟踪：性能测试
   - 应对：持续优化

2. 并发问题
   - 跟踪：压力测试
   - 应对：完善测试

### 7.2 进度风险
1. 人力资源
   - 跟踪：周进度
   - 应对：调整优先级

2. 技术障碍
   - 跟踪：技术评审
   - 应对：及时调整
``` 