# InfraxAsync 重构方案

## 设计目标
1. 统一所有异步操作的接口，使用统一的 poll 机制
2. 移除 setjmp/longjmp 和 ucontext 相关代码
3. 简化协程实现，提高可维护性

## 核心设计
1. 事件源抽象
```c
typedef struct {
    void* source;              // 事件源
    int (*ready)(void* source);  // 检查是否就绪
    int (*wait)(void* source);   // 等待就绪
    void (*cleanup)(void* source); // 清理资源
} InfraxEventSource;
```

2. 协程状态简化
```c
typedef enum {
    ASYNC_INIT,      // 初始状态
    ASYNC_READY,     // 就绪状态
    ASYNC_WAITING,   // 等待事件
    ASYNC_DONE       // 完成状态
} AsyncState;
```

## 实现步骤
1. ✅ 修改 InfraxAsync.h
   - 添加事件源相关定义
   - 简化协程状态
   - 移除 jmp 相关定义

2. ✅ 修改 InfraxAsync.c
   - 移除 jmp 相关代码
   - 实现事件源机制
   - 修改调度器实现

3. ✅ 适配现有功能
   - IO事件适配
   - Timer事件适配
   - 其他事件适配

## 测试计划
1. 基础功能测试
   - 定时器事件测试
   - IO 事件测试
   - 多协程并发测试
   - 错误处理测试

2. 压力测试
   - 多协程并发
   - 长时间运行
   - 资源泄漏检查

## 测试结果记录
1. 编译测试结果
2. 单元测试结果
3. 压力测试结果分析

## 注意事项
1. 保持向后兼容性
2. 确保线程安全
3. 优化性能
4. 完善错误处理
