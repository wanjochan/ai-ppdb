# engine layer Documentation

## Overview

The engine layer provides fundamental building blocks for the PPDB database system, including:

- Synchronization primitives
- Asynchronous I/O
- Performance monitoring
- Memory management

## Synchronization Primitives

### Mutex

```c
ppdb_error_t ppdb_engine_mutex_create(ppdb_engine_mutex_t** mutex);
ppdb_error_t ppdb_engine_mutex_destroy(ppdb_engine_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_lock(ppdb_engine_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_unlock(ppdb_engine_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_trylock(ppdb_engine_mutex_t* mutex);
```

Provides basic mutual exclusion with both blocking and non-blocking operations.

### RWLock

```c
ppdb_error_t ppdb_engine_rwlock_create(ppdb_engine_rwlock_t** lock);
ppdb_error_t ppdb_engine_rwlock_destroy(ppdb_engine_rwlock_t* lock);
ppdb_error_t ppdb_engine_rwlock_rdlock(ppdb_engine_rwlock_t* lock);
ppdb_error_t ppdb_engine_rwlock_wrlock(ppdb_engine_rwlock_t* lock);
ppdb_error_t ppdb_engine_rwlock_unlock(ppdb_engine_rwlock_t* lock);
```

Reader-writer lock for concurrent read access with exclusive write access.

### Semaphore

```c
ppdb_error_t ppdb_engine_sem_create(ppdb_engine_sem_t** sem, size_t initial_value);
ppdb_error_t ppdb_engine_sem_destroy(ppdb_engine_sem_t* sem);
ppdb_error_t ppdb_engine_sem_wait(ppdb_engine_sem_t* sem);
ppdb_error_t ppdb_engine_sem_post(ppdb_engine_sem_t* sem);
```

Counting semaphore for resource management and synchronization.

## Asynchronous I/O

### Event Loop

```c
ppdb_error_t ppdb_engine_async_loop_create(ppdb_engine_async_loop_t** loop);
ppdb_error_t ppdb_engine_async_loop_destroy(ppdb_engine_async_loop_t* loop);
ppdb_error_t ppdb_engine_async_loop_run(ppdb_engine_async_loop_t* loop, int timeout_ms);
```

Event loop for asynchronous I/O operations.

### Timer

```c
ppdb_error_t ppdb_engine_timer_create(ppdb_engine_async_loop_t* loop, ppdb_engine_timer_t** timer);
ppdb_error_t ppdb_engine_timer_destroy(ppdb_engine_timer_t* timer);
ppdb_error_t ppdb_engine_timer_start(ppdb_engine_timer_t* timer, uint64_t timeout_ms, bool repeat);
```

High-resolution timer support.

### Future

```c
ppdb_error_t ppdb_engine_future_create(ppdb_engine_async_loop_t* loop, ppdb_engine_future_t** future);
ppdb_error_t ppdb_engine_future_destroy(ppdb_engine_future_t* future);
ppdb_error_t ppdb_engine_future_wait(ppdb_engine_future_t* future);
```

Future pattern for asynchronous results.

## Performance Monitoring

### Counters

```c
ppdb_error_t ppdb_engine_perf_counter_create(const char* name, ppdb_engine_perf_counter_t** counter);
ppdb_error_t ppdb_engine_perf_counter_increment(ppdb_engine_perf_counter_t* counter);
ppdb_error_t ppdb_engine_perf_counter_add(ppdb_engine_perf_counter_t* counter, size_t value);
```

Performance counters for metrics collection.

### Timers

```c
ppdb_error_t ppdb_engine_perf_timer_start(ppdb_engine_perf_counter_t* counter, ppdb_engine_perf_timer_t** timer);
ppdb_error_t ppdb_engine_perf_timer_stop(ppdb_engine_perf_timer_t* timer);
```

High-precision timing measurements.

## Platform Support

### Windows

- IOCP for efficient async I/O
- Native semaphores
- High-resolution timers

### Linux

- epoll for event notification
- POSIX semaphores
- timerfd support

## Best Practices

1. **Memory Management**
   - Use aligned allocation for better performance
   - Free resources in reverse order of acquisition
   - Check return values for all allocations

2. **Synchronization**
   - Prefer reader-writer locks for read-heavy workloads
   - Use trylock to avoid deadlocks
   - Keep critical sections small

3. **Async I/O**
   - Use edge-triggered mode for better performance
   - Set appropriate buffer sizes
   - Handle partial reads/writes

4. **Performance Monitoring**
   - Create counters with descriptive names
   - Use timers for critical paths
   - Regular performance reporting

## Error Handling

All functions return `ppdb_error_t` with the following values:

- `PPDB_OK`: Success
- `PPDB_ERR_NULL_POINTER`: NULL argument
- `PPDB_ERR_INVALID_ARGUMENT`: Invalid parameter
- `PPDB_ERR_OUT_OF_MEMORY`: Memory allocation failed
- `PPDB_ERR_INTERNAL`: Internal error
- `PPDB_ERR_TIMEOUT`: Operation timed out
- `PPDB_ERR_WOULD_BLOCK`: Non-blocking operation would block

## Thread Safety

All engine layer functions are thread-safe unless explicitly documented otherwise.
Internal data structures use appropriate synchronization mechanisms to ensure
thread safety without sacrificing performance.
