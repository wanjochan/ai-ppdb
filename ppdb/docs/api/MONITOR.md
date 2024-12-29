# PPDB 性能监控 API

## 概述
本文档描述了 PPDB 的性能监控 API，包括性能指标收集、分析和自适应切换功能。

## API 结构

### 性能指标
```c
typedef struct ppdb_perf_metrics {
    atomic_uint_least64_t op_count;           // 操作总数
    atomic_uint_least64_t total_latency_us;   // 总延迟（微秒）
    atomic_uint_least64_t max_latency_us;     // 最大延迟（微秒）
    atomic_uint_least64_t lock_contentions;   // 锁竞争次数
    atomic_uint_least64_t lock_wait_us;       // 锁等待时间
} ppdb_perf_metrics_t;
```

### 监控器
```c
typedef struct ppdb_monitor {
    ppdb_perf_metrics_t current;              // 当前窗口指标
    ppdb_perf_metrics_t previous;             // 上一窗口指标
    atomic_bool should_switch;                // 是否需要切换标志
    time_t window_start_ms;                   // 当前窗口开始时间
    int cpu_cores;                            // CPU核心数
} ppdb_monitor_t;
```

## 功能接口

### 监控器管理
```c
// 创建监控器
ppdb_monitor_t* ppdb_monitor_create(void);

// 销毁监控器
void ppdb_monitor_destroy(ppdb_monitor_t* monitor);
```

### 性能指标收集
```c
// 记录操作开始
void ppdb_monitor_op_start(ppdb_monitor_t* monitor);

// 记录操作结束
void ppdb_monitor_op_end(ppdb_monitor_t* monitor, uint64_t latency_us);

// 记录锁竞争
void ppdb_monitor_lock_contention(ppdb_monitor_t* monitor, uint64_t wait_us);
```

### 指标查询
```c
// 获取当前QPS
uint64_t ppdb_monitor_get_qps(ppdb_monitor_t* monitor);

// 获取当前P99延迟（微秒）
uint64_t ppdb_monitor_get_p99_latency(ppdb_monitor_t* monitor);

// 获取锁竞争率（百分比）
double ppdb_monitor_get_contention_rate(ppdb_monitor_t* monitor);
```

### 自适应控制
```c
// 检查是否需要切换到分片模式
bool ppdb_monitor_should_switch(ppdb_monitor_t* monitor);
```

## 使用示例

### 基本使用
```c
// 创建监控器
ppdb_monitor_t* monitor = ppdb_monitor_create();

// 记录操作
ppdb_monitor_op_start(monitor);
// ... 执行操作 ...
ppdb_monitor_op_end(monitor, latency_us);

// 检查性能状态
if (ppdb_monitor_should_switch(monitor)) {
    // 切换到分片模式
}

// 清理
ppdb_monitor_destroy(monitor);
```

### 性能分析
```c
// 获取性能指标
uint64_t qps = ppdb_monitor_get_qps(monitor);
uint64_t p99 = ppdb_monitor_get_p99_latency(monitor);
double contention = ppdb_monitor_get_contention_rate(monitor);

printf("Performance Metrics:\n");
printf("QPS: %lu\n", qps);
printf("P99 Latency: %lu us\n", p99);
printf("Lock Contention: %.2f%%\n", contention);
```

## 切换条件

系统会在满足以下任一条件时自动切换到分片模式：

1. 性能条件
   - QPS > 50,000
   - P99 延迟 > 5ms
   - 锁竞争率 > 30%

2. 硬件条件
   - CPU 核心数 >= 8

## 监控窗口

- 默认窗口大小：1000ms（1秒）
- 窗口滑动：每秒更新一次
- 指标重置：窗口切换时自动重置

## 注意事项

1. 线程安全
   - 所有接口都是线程安全的
   - 使用原子操作保证数据一致性
   - 无需额外加锁

2. 性能影响
   - 监控开销很小
   - 使用无锁计数器
   - 异步统计分析

3. 内存使用
   - 每个监控器约占用 64 字节
   - 不会动态分配额外内存
   - 资源占用可预测

4. 最佳实践
   - 及时处理切换信号
   - 定期检查性能指标
   - 在日志中记录重要事件
