# PPDB

PPDB是一个高性能的分布式数据库系统。

## 特性

- 高性能：使用异步IO和事件驱动架构
- 跨平台：支持Windows（IOCP）和Linux（epoll）
- 可扩展：模块化设计，易于扩展
- 可靠性：完善的错误处理和监控

## 架构

### Base层

Base层提供了基础设施支持，包括：

1. 异步IO系统
   - IO管理器：优先级队列和动态线程池
   - 事件系统：IOCP/epoll事件处理
   - 定时器：纳秒级精度和优先级支持

2. 同步原语
   - 自适应自旋锁
   - 读写锁（支持超时）
   - 互斥锁和条件变量

3. 内存管理
   - 内存池
   - 对象缓存
   - 内存跟踪

4. 核心功能
   - 错误处理
   - 日志系统
   - 性能统计

## 编译和安装

### 依赖

- cosmopolitan libc
- cmake >= 3.10
- gcc >= 8.0 或 clang >= 10.0

### 编译步骤

```bash
mkdir build
cd build
cmake ..
make
```

## 使用示例

### 异步IO示例

```c
// 创建IO管理器
ppdb_base_io_manager_t* mgr;
ppdb_base_io_manager_create(&mgr, 1024, 4);

// 添加高优先级IO请求
ppdb_base_io_manager_schedule_priority(mgr, io_handler, data, PPDB_IO_PRIORITY_HIGH);

// 清理
ppdb_base_io_manager_destroy(mgr);
```

### 事件处理示例

```c
// 创建事件循环
ppdb_base_event_loop_t* loop;
ppdb_base_event_loop_create(&loop);

// 添加事件处理器
ppdb_base_event_handler_create(loop, &handler, fd,
    PPDB_EVENT_READ | PPDB_EVENT_WRITE,
    event_callback, NULL);

// 运行事件循环
ppdb_base_event_loop_run(loop, 1000);
```

### 定时器示例

```c
// 创建定时器
ppdb_base_timer_t* timer;
ppdb_base_timer_create(&timer, 100,
    TIMER_FLAG_REPEAT | TIMER_FLAG_PRECISE,
    TIMER_PRIORITY_HIGH, timer_callback, NULL);

// 启动定时器
ppdb_base_timer_start(timer);
```

## 文档

- [API文档](docs/api/base.md)
- [设计文档](docs/design/base.md)
- [测试文档](docs/test/base.md)

## 贡献

欢迎提交Pull Request和Issue。

## 许可证

MIT License 