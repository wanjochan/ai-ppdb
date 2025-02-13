# PolyxAsync Poll场景扩展方案

## 问题分析
1. 现有代码已支持基础事件类型：
   - POLYX_EVENT_NONE
   - POLYX_EVENT_IO
   - POLYX_EVENT_TIMER
   - POLYX_EVENT_SIGNAL

2. poll()机制支持但尚未实现的场景：
   - 网络相关：TCP、UDP、Unix domain sockets
   - 标准IO：管道、FIFO、终端设备
   - 其他：字符设备、inotify等

## 已完成的扩展

### 1. 事件类型和状态管理
- 新增网络、IO和监控事件类型
- 添加事件状态管理(INIT/READY/ACTIVE/PAUSED/ERROR/CLOSED)
- 定义错误码系统

### 2. 配置结构优化
- 网络配置细化：支持TCP/UDP/Unix特定选项
- IO配置：支持通用文件描述符操作
- 文件监控配置：支持inotify功能

### 3. 数据结构优化
- 使用union高效存储不同类型事件数据
- 事件结构增加状态和错误信息
- 改进类型安全性

### 4. 辅助功能增强
- 更精确的事件类型检查宏
- 增加事件状态检查宏
- 添加可读性/可写性检查函数
- 提供状态和错误信息转换函数

### 5. 调试和统计支持
- 添加事件统计结构(PolyxEventStats)
- 添加调试级别和回调机制
- 提供调试宏简化日志输出
- 支持事件组管理

## 测试计划

### 1. 基础功能测试
```c
void test_polyx_async_basic(void) {
    PolyxAsync* async = PolyxAsyncClass.new();
    assert(async != NULL);
    
    // Test event creation
    PolyxEventConfig config = {
        .type = POLYX_EVENT_IO,
        .callback = NULL,
        .arg = NULL
    };
    PolyxEvent* event = async->create_event(async, &config);
    assert(event != NULL);
    assert(event->type == POLYX_EVENT_IO);
    assert(event->status == POLYX_EVENT_STATUS_INIT);
    
    async->destroy_event(async, event);
    PolyxAsyncClass.free(async);
}
```

### 2. 网络事件测试
```c
void test_polyx_async_network(void) {
    PolyxAsync* async = PolyxAsyncClass.new();
    
    // Test TCP event
    PolyxNetworkConfig tcp_config = {
        .socket_fd = -1,
        .events = POLLIN | POLLOUT,
        .protocol_opts.tcp = {
            .backlog = 5,
            .reuse_addr = true
        }
    };
    PolyxEvent* tcp_event = async->create_tcp_event(async, &tcp_config);
    assert(tcp_event != NULL);
    assert(POLYX_EVENT_IS_NETWORK(tcp_event));
    
    async->destroy_event(async, tcp_event);
    PolyxAsyncClass.free(async);
}
```

### 3. 调试功能测试
```c
static void debug_callback(PolyxDebugLevel level, const char* file, int line, 
                         const char* func, const char* msg) {
    printf("[%s:%d] %s: %s\n", file, line, func, msg);
}

void test_polyx_async_debug(void) {
    PolyxAsync* async = PolyxAsyncClass.new();
    
    async->set_debug_level(async, POLYX_DEBUG_INFO);
    async->set_debug_callback(async, debug_callback, NULL);
    
    POLYX_INFO(async, "Debug test message");
    
    PolyxAsyncClass.free(async);
}
```

## 下一步计划

1. 实现 PolyxAsync.c 中的具体功能：
   - 事件创建和销毁
   - 事件状态管理
   - 事件组管理
   - 调试和统计功能

2. 编写完整的测试用例：
   - 基础功能测试
   - 网络功能测试
   - IO功能测试
   - 调试功能测试

3. 运行测试并修复问题：
   ```bash
   timeout 20s sh ppx/scripts/build_test_arch.sh test_polyx_async
   ```

## 注意事项
1. 确保内存管理正确性
2. 保证线程安全性
3. 处理所有错误情况
4. 完善调试信息输出

## 遗留问题
1. 是否需要添加事件统计功能
2. 是否需要添加更多的事件转换函数
3. 是否需要支持事件组(event group)功能
4. 是否需要添加调试日志支持 