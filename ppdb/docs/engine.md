# PPDB 引擎层文档

## 概述

引擎层为PPDB数据库系统提供基础构建块，包括：

- 同步原语
- 异步I/O
- 性能监控
- 内存管理

## 同步原语

### 互斥锁（Mutex）

```c
ppdb_error_t ppdb_engine_mutex_create(ppdb_engine_mutex_t** mutex);
ppdb_error_t ppdb_engine_mutex_destroy(ppdb_engine_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_lock(ppdb_engine_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_unlock(ppdb_engine_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_trylock(ppdb_engine_mutex_t* mutex);
```

提供基本的互斥操作，支持阻塞和非阻塞操作。

### 读写锁（RWLock）

```c
ppdb_error_t ppdb_engine_rwlock_create(ppdb_engine_rwlock_t** lock);
ppdb_error_t ppdb_engine_rwlock_destroy(ppdb_engine_rwlock_t* lock);
ppdb_error_t ppdb_engine_rwlock_rdlock(ppdb_engine_rwlock_t* lock);
ppdb_error_t ppdb_engine_rwlock_wrlock(ppdb_engine_rwlock_t* lock);
ppdb_error_t ppdb_engine_rwlock_unlock(ppdb_engine_rwlock_t* lock);
```

读写锁支持并发读访问和独占写访问。

### 信号量（Semaphore）

```c
ppdb_error_t ppdb_engine_sem_create(ppdb_engine_sem_t** sem, size_t initial_value);
ppdb_error_t ppdb_engine_sem_destroy(ppdb_engine_sem_t* sem);
ppdb_error_t ppdb_engine_sem_wait(ppdb_engine_sem_t* sem);
ppdb_error_t ppdb_engine_sem_post(ppdb_engine_sem_t* sem);
```

计数信号量用于资源管理和同步。

## 异步I/O

### 事件循环

```c
ppdb_error_t ppdb_engine_async_loop_create(ppdb_engine_async_loop_t** loop);
ppdb_error_t ppdb_engine_async_loop_destroy(ppdb_engine_async_loop_t* loop);
ppdb_error_t ppdb_engine_async_loop_run(ppdb_engine_async_loop_t* loop, int timeout_ms);
```

用于异步I/O操作的事件循环。

### 定时器

```c
ppdb_error_t ppdb_engine_timer_create(ppdb_engine_async_loop_t* loop, ppdb_engine_timer_t** timer);
ppdb_error_t ppdb_engine_timer_destroy(ppdb_engine_timer_t* timer);
ppdb_error_t ppdb_engine_timer_start(ppdb_engine_timer_t* timer, uint64_t timeout_ms, bool repeat);
```

高精度定时器支持。

### Future模式

```c
ppdb_error_t ppdb_engine_future_create(ppdb_engine_async_loop_t* loop, ppdb_engine_future_t** future);
ppdb_error_t ppdb_engine_future_destroy(ppdb_engine_future_t* future);
ppdb_error_t ppdb_engine_future_wait(ppdb_engine_future_t* future);
```

用于异步结果的Future模式。

## 性能监控

### 计数器

```c
ppdb_error_t ppdb_engine_perf_counter_create(const char* name, ppdb_engine_perf_counter_t** counter);
ppdb_error_t ppdb_engine_perf_counter_increment(ppdb_engine_perf_counter_t* counter);
ppdb_error_t ppdb_engine_perf_counter_add(ppdb_engine_perf_counter_t* counter, size_t value);
```

用于指标收集的性能计数器。

### 计时器

```c
ppdb_error_t ppdb_engine_perf_timer_start(ppdb_engine_perf_counter_t* counter, ppdb_engine_perf_timer_t** timer);
ppdb_error_t ppdb_engine_perf_timer_stop(ppdb_engine_perf_timer_t* timer);
```

高精度时间测量。

## 平台支持

### Windows

- 使用IOCP实现高效异步I/O
- 原生信号量
- 高精度定时器

### Linux

- 使用epoll进行事件通知
- POSIX信号量
- timerfd支持

## 最佳实践

1. **内存管理**
   - 使用对齐分配以提高性能
   - 按照资源获取的相反顺序释放
   - 检查所有分配的返回值

2. **同步**
   - 读多写少场景优先使用读写锁
   - 使用trylock避免死锁
   - 保持临界区小巧

3. **异步I/O**
   - 使用边缘触发模式以提高性能
   - 设置合适的缓冲区大小
   - 处理部分读写情况

4. **性能监控**
   - 创建具有描述性名称的计数器
   - 在关键路径使用计时器
   - 定期性能报告

## 错误处理

所有函数返回`ppdb_error_t`，可能的值包括：

- `PPDB_OK`：成功
- `PPDB_ERR_NULL_POINTER`：空指针参数
- `PPDB_ERR_INVALID_ARGUMENT`：无效参数
- `PPDB_ERR_OUT_OF_MEMORY`：内存分配失败
- `PPDB_ERR_INTERNAL`：内部错误
- `PPDB_ERR_TIMEOUT`：操作超时
- `PPDB_ERR_WOULD_BLOCK`：非阻塞操作会阻塞

## 线程安全

除非特别说明，引擎层的所有函数都是线程安全的。
内部数据结构使用适当的同步机制来确保线程安全，同时不牺牲性能。
