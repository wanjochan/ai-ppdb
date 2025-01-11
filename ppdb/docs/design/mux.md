# 多路复用模块设计文档

## 1. 背景

在高性能服务器开发中，需要高效处理大量并发I/O操作。不同平台提供了不同的多路复用机制：
- Linux: epoll
- Windows: IOCP (I/O Completion Port)
- 通用: select (用于调试)

为了统一这些接口，同时保持高性能，我们设计了这个多路复用抽象层。

## 2. 设计目标

1. 提供统一的跨平台接口
2. 保持原生性能
3. 支持不同类型的I/O操作
4. 易于使用和维护
5. 可配置和可扩展

## 3. 核心接口

```c
// 创建多路复用上下文
infra_error_t infra_mux_create(infra_mux_type_t type, infra_mux_ctx_t** ctx);

// 销毁上下文
infra_error_t infra_mux_destroy(infra_mux_ctx_t* ctx);

// 添加文件描述符
infra_error_t infra_mux_add(infra_mux_ctx_t* ctx, int fd, infra_event_type_t events, void* user_data);

// 移除文件描述符
infra_error_t infra_mux_remove(infra_mux_ctx_t* ctx, int fd);

// 修改监听的事件
infra_error_t infra_mux_modify(infra_mux_ctx_t* ctx, int fd, infra_event_type_t events);

// 等待事件
infra_error_t infra_mux_wait(infra_mux_ctx_t* ctx, infra_mux_event_t* events, size_t max_events, int timeout_ms);
```

## 4. I/O支持范围

### 4.1 Windows (IOCP)
- Socket I/O: ✓ 完全支持
- Disk I/O: ✓ 完全支持（需要以异步模式打开文件）
- 命名管道: ✓ 支持
- 其他设备: △ 部分支持

### 4.2 Linux (epoll)
- Socket I/O: ✓ 完全支持
- Disk I/O: △ 有限支持（建议使用独立线程池）
- Pipe/FIFO: ✓ 支持
- 终端设备: ✓ 支持
- 信号: ✓ 支持（通过signalfd）
- 定时器: ✓ 支持（通过timerfd）

## 5. 使用示例

### 5.1 简单的时间查询服务器
```c
int main() {
    // 创建多路复用上下文
    infra_mux_ctx_t* mux_ctx;
    infra_mux_create(INFRA_MUX_AUTO, &mux_ctx);
    
    // 创建监听socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    bind(listen_fd, ...);
    listen(listen_fd, 128);
    
    // 添加到多路复用
    infra_mux_add(mux_ctx, listen_fd, INFRA_EVENT_READ, NULL);
    
    // 事件循环
    infra_mux_event_t events[32];
    while (1) {
        int n = infra_mux_wait(mux_ctx, events, 32, 1000);
        for (int i = 0; i < n; i++) {
            // 处理事件...
        }
    }
    
    infra_mux_destroy(mux_ctx);
    return 0;
}
```

### 5.2 配合线程池使用
```c
int main() {
    thread_pool_t* pool = thread_pool_create(4);
    infra_mux_ctx_t* mux_ctx;
    infra_mux_create(INFRA_MUX_AUTO, &mux_ctx);
    
    while (1) {
        infra_mux_event_t events[32];
        int n = infra_mux_wait(mux_ctx, events, 32, 1000);
        
        for (int i = 0; i < n; i++) {
            thread_pool_submit(pool, handle_event, &events[i]);
        }
    }
}
```

## 6. 最佳实践

1. **Socket I/O处理**:
   - 使用非阻塞模式
   - 合理设置缓冲区大小
   - 注意处理EAGAIN错误

2. **磁盘I/O处理**:
   - Windows: 使用异步模式打开文件
   - Linux: 使用独立的线程池或libaio

3. **性能优化**:
   - 合理设置max_events大小
   - 使用边缘触发模式(epoll)
   - 避免频繁添加/删除描述符

4. **错误处理**:
   - 总是检查返回值
   - 正确清理资源
   - 处理断开连接的情况

## 7. 注意事项

1. 不同平台的特性差异
2. 文件描述符限制
3. 内存使用考虑
4. 错误处理策略
5. 性能监控和调优 