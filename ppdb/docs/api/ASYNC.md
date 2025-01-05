# PPDB Async Library

PPDB 的异步库提供高性能�?I/O 多路复用机制�?
## 设计目标

1. **简单�?*
   - 简洁的 API 设计
   - 最小化依赖
   - 易于理解和使�?
2. **可扩展�?*
   - 抽象�?I/O 后端接口
   - 支持多种 I/O 多路复用机制
   - 预留扩展接口

3. **高性能**
   - 事件驱动架构
   - 非阻�?I/O
   - 批量操作支持

## 核心组件

### 1. Event Loop
```c
async_loop_t* loop = async_loop_new();
async_loop_run(loop, timeout_ms);
```

### 2. Handle
```c
async_handle_t* handle = async_handle_new(loop, fd);
async_handle_read(handle, buf, len, callback);
```

### 3. Callback
```c
void on_read(async_handle_t* handle, int status) {
    // 处理读取结果
}
```

## I/O 后端

当前实现：使�?poll 机制
```c
static int poll_wait(void* data, int timeout_ms) {
    poll_backend_t* backend = data;
    return poll(backend->fds, backend->size, timeout_ms);
}
```

计划支持�?- Windows: IOCP
- Linux: epoll
- BSD: kqueue

## 使用示例

### 1. 基本用法
```c
void on_data(async_handle_t* handle, int status) {
    printf("Read %d bytes\n", status);
}

int main() {
    async_loop_t* loop = async_loop_new();
    async_handle_t* handle = async_handle_new(loop, fd);
    
    async_handle_read(handle, buf, 1024, on_data);
    async_loop_run(loop, -1);
}
```

### 2. 服务器示�?```c
void on_client(async_handle_t* handle, int status) {
    int client_fd = accept(server_fd, NULL, NULL);
    async_handle_t* client = async_handle_new(loop, client_fd);
    async_handle_read(client, buf, len, on_data);
}
```

## 性能优化

1. **批量操作**
   - 收集多个操作一起处�?   - 减少系统调用
   - 提高吞吐�?
2. **零拷�?*
   - 直接 I/O
   - 内存映射
   - 减少数据复制

3. **线程�?*
   - 处理计算密集任务
   - 保持事件循环响应

## 最佳实�?
1. **错误处理**
   - 始终检查返回�?   - 正确处理回调中的错误
   - 资源清理

2. **资源管理**
   - 及时释放 handle
   - 避免内存泄漏
   - 合理设置缓冲区大�?
3. **性能调优**
   - 使用合适的缓冲区大�?   - 批量处理请求
   - 监控系统资源使用
