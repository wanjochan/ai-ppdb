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

- [ ] WAL改进
  - [ ] 基于段的管理
  - [ ] 压缩和清理
  - [x] 同步/异步配置
  ```c
  typedef struct wal_segment {
      uint64_t id;
      size_t size;
      bool is_active;
      char* filename;
  } wal_segment_t;
  ```

### 1.2 并发控制
- [x] 同步原语重构
  ```c
  typedef struct ppdb_sync {
      union {
          atomic_int atomic;
          mutex_t mutex;
      } impl;
      ppdb_sync_config_t config;
      ppdb_ref_count_t* ref_count;
  } ppdb_sync_t;
  ```
  - [x] 无锁/互斥模式
  - [x] 分片锁优化
  - [x] 引用计数管理

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
  - [ ] 目录重组
  - [ ] 模块解耦
  - [ ] 压缩模块独立

- [ ] 规范统一
  - [ ] 代码风格
  - [ ] 错误处理
  - [ ] 日志格式

### 3.2 构建系统
- [ ] 编译配置
  - [ ] 路径处理
  - [ ] 编译选项
  - [ ] 版本管理

## 4. 问题跟踪

### 4.1 严重问题
- [ ] WAL实现完善
  - [ ] 段管理功能
  - [ ] 压缩清理机制
  - [ ] 配置系统

- [ ] 监控系统完善
  - [ ] 指标扩展
  - [ ] 监控集成
  - [ ] 性能分析

### 4.2 中等问题
- [ ] 内存优化
  - [ ] 数据结构改进
  - [ ] 监控机制
  - [ ] 缓存策略

- [ ] 构建优化
  - [ ] 路径处理
  - [ ] 编译系统
  - [ ] 版本注入

### 4.3 轻微问题
- [ ] 文档更新
  - [ ] 设计文档
  - [ ] API文档
  - [ ] 运维指南

- [ ] 其他改进
  - [ ] 基准测试
  - [ ] 错误码
  - [ ] 日志规范
