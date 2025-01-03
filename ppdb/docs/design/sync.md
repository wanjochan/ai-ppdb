# 同步原语设计文档

## 1. 设计目标

同步原语作为 PPDB 的基础设施层核心组件，主要目标是：
1. 提供高性能的并发控制机制
2. 支持无锁算法实现
3. 确保线程安全的数据访问
4. 最小化同步开销

## 2. 当前实现状态

### 2.1 已实现功能

#### 基础同步类型
- 互斥锁 (Mutex)
  * 标准互斥锁实现
  * pthread 和无锁两种模式
  * 可配置的自旋等待
  * 支持公平性调度

- 自旋锁 (Spinlock)
  * 基于原子操作
  * 可配置自旋次数
  * 支持退避策略

- 读写锁 (RWLock)
  * 读写分离实现
  * 最大读者数限制
  * 写优先策略
  * 原子状态管理

#### 原子操作支持
- CAS (Compare-And-Swap)
- FAA (Fetch-And-Add)
- 内存序保证
- 原子位操作

#### 性能优化
- 自适应自旋
- 指数退避策略
- 缓存对齐
- 性能统计收集

### 2.2 开发中功能

#### 分片锁（部分实现）
- [x] 基础框架搭建
- [ ] 自适应分片算法
- [ ] 动态调整策略
- [ ] 性能监控

#### 引用计数（计划中）
- [ ] 智能指针封装
- [ ] 原子引用计数
- [ ] 自动内存管理
- [ ] 循环引用检测

### 2.3 计划功能
- 条件变量支持
- 死锁检测
- 更多性能分析工具
- NUMA 感知优化

## 3. API 使用示例

### 3.1 基本互斥锁
```c
ppdb_sync_config_t config = {
    .type = PPDB_SYNC_MUTEX,
    .use_lockfree = true,
    .backoff_us = 100,
    .max_retries = 10
};

ppdb_sync_t* sync;
ppdb_sync_create(&sync, &config);

// 使用锁
if (ppdb_sync_try_lock(sync) == PPDB_OK) {
    // 临界区操作
    ppdb_sync_unlock(sync);
}
```

### 3.2 读写锁
```c
ppdb_sync_config_t config = {
    .type = PPDB_SYNC_RWLOCK,
    .max_readers = 32,
    .backoff_us = 100,
    .max_retries = 10
};

ppdb_sync_t* sync;
ppdb_sync_create(&sync, &config);

// 读操作
if (ppdb_sync_read_lock(sync) == PPDB_OK) {
    // 读取操作
    ppdb_sync_read_unlock(sync);
}

// 写操作
if (ppdb_sync_write_lock(sync) == PPDB_OK) {
    // 写入操作
    ppdb_sync_write_unlock(sync);
}
```

## 4. 性能特性

### 4.1 基准测试结果
- 读写锁并发性能（32读者/8写者）：
  * 读操作吞吐量：~100万次/秒
  * 写操作吞吐量：~10万次/秒
  * 平均锁等待时间：< 1微秒
  * 锁竞争率：< 5%

### 4.2 优化策略
- 快速路径优化
  * 无竞争时零等待
  * 原子操作最小化
  * 缓存行对齐

- 竞争处理
  * 自适应自旋
  * 指数退避
  * 公平性保证

## 5. 注意事项

### 5.1 使用建议
1. 优先使用无锁模式（设置 `use_lockfree = true`）
2. 合理配置最大读者数量（`max_readers`）
3. 根据场景调整退避参数（`backoff_us`）
4. 避免长时间持有写锁

### 5.2 已知限制
1. 分片锁功能尚未完全实现
2. 不支持递归锁
3. 写优先可能导致读饥饿
4. 暂不支持条件变量
