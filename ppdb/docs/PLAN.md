# PPDB 开发计划

## 1. 核心功能开发

### 1.1 存储引擎
- [x] 无锁跳表实现
  - [x] 节点内存对齐
  - [x] 原子操作优化
  - [x] 迭代器实现

- [ ] 内存表优化
  - [x] 分片策略完善
  - [ ] 紧凑数据结构
  - [ ] 缓存共享机制
  - [x] 无锁操作接口
  - [x] 内存表满处理机制
  - [x] 自动切换策略
  - [ ] 修复类型定义问题
    - [ ] 完善 skiplist.h 接口
    - [ ] 修复 metrics.h 中的原子类型
    - [ ] 统一函数签名

- [ ] WAL改进
  - [ ] 基于段的管理
  - [ ] 压缩和清理
  - [x] 同步/异步配置
  - [x] 无锁操作接口

### 1.2 并发控制
- [ ] 同步原语重构
  ```c
  typedef struct ppdb_sync {
      mutex_t mutex;
      atomic_bool is_locked;
  } ppdb_sync_t;
  ```
  - [x] 基础同步类型实现
    - [x] mutex_t 类型定义
    - [x] mutex 基本操作
    - [x] 原子操作封装
  - [x] 统一使用 ppdb_sync_t
    - [x] KVStore 同步改造
    - [ ] MemTable 同步改造
    - [ ] WAL 同步改造
  - [ ] 分片锁优化
  - [ ] 引用计数管理

- [ ] 内存管理改进
  ```c
  typedef struct ppdb_mempool {
      void* chunks;
      size_t chunk_size;
      atomic_size_t used;
      ppdb_stripe_locks_t* locks;
  } ppdb_mempool_t;
  ```
  - [ ] 内存池优化
  - [ ] 资源限制
  - [ ] 垃圾回收

## 2. 可靠性保障

### 2.1 监控系统
- [ ] 核心指标
  ```c
  typedef struct ppdb_extended_metrics {
      atomic_uint64_t wal_write_latency;
      atomic_uint64_t compression_ratio;
      atomic_uint64_t cache_hit_rate;
      atomic_uint64_t memory_usage;
  } ppdb_extended_metrics_t;
  ```
  - [x] 性能计数器
  - [ ] Prometheus集成
  - [ ] 告警机制
  - [x] 基础监控接口

### 2.2 测试覆盖
- [ ] 基础测试
  - [ ] 单元测试补充
  - [ ] 集成测试
  - [ ] 边界测试

- [ ] 高级测试
  - [ ] 并发测试
  - [ ] 压力测试
  - [ ] 恢复测试

## 3. 工程优化

### 3.1 代码质量
- [ ] 结构优化
  - [x] 接口规范化
  - [ ] 模块解耦
  - [ ] 压缩模块独立

- [ ] 规范统一
  - [ ] 代码风格
  - [x] 错误处理
  - [ ] 日志格式

### 3.2 构建系统
- [x] 编译配置
  - [x] 路径处理
  - [x] 编译选项
  - [ ] 版本管理
  - [x] 工具链检查
  - [x] 依赖管理

- [ ] 构建环境
  - [x] 交叉编译工具链
  - [x] 第三方库管理
  - [ ] 环境变量配置

- [ ] 构建脚本优化
  - [ ] 错误处理完善
  - [ ] 清理流程优化
  - [ ] 增量构建支持

## 4. 下一步任务

### 4.1 紧急任务
1. 完成同步原语统一
   - [x] KVStore 使用 ppdb_sync_t
   - [ ] MemTable 使用 ppdb_sync_t
   - [ ] WAL 使用 ppdb_sync_t
   - [ ] 其他组件同步改造

2. 修复内存表相关问题
   - 完善 skiplist.h 接口定义
   - 修复 metrics.h 中的原子类型
   - 统一函数签名
   - 实现缺失的函数
   - 修复返回值问题

3. 实现无锁版本的函数
   - memtable_put_lockfree
   - memtable_get_lockfree
   - memtable_delete_lockfree
   - wal_write_lockfree
   - wal_recover_lockfree

4. 完善监控系统实现
   - monitor_op_start/end
   - monitor_should_switch
   - 性能指标收集

### 4.2 待办任务
1. 补充单元测试
2. 实现压缩模块
3. 完善日志系统
4. 添加性能基准测试
