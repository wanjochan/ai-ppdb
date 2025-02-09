# 线程池实现计划

## 问题分析

1. 结构体定义问题
   - InfraxThreadConfig 缺少必要字段
   - InfraxThread 缺少运行时状态
   - 错误码定义不统一

2. 同步原语问题
   - 直接使用 pthread 函数而不是 InfraxSync
   - 同步对象生命周期管理不当

3. 内存管理问题
   - 未使用 InfraxCore 的内存管理接口
   - 内存分配失败处理不完整

4. 错误处理问题
   - 错误码定义不统一
   - 错误创建方式不规范

5. 线程池设计问题
   - 数据结构设计不完整
   - 共享数据保护不足

## 解决方案

### 1. 结构体重新定义

```c
// 线程配置
typedef struct {
    const char* name;           // 线程名称
    InfraxThreadFunc func;      // 线程函数
    void* arg;                  // 线程参数
    size_t stack_size;         // 栈大小(可选)
    int priority;              // 优先级(可选)
} InfraxThreadConfig;

// 线程结构体
typedef struct InfraxThread {
    void* native_handle;       // 底层线程句柄
    InfraxThreadConfig config; // 线程配置
    bool is_running;          // 运行状态
    void* result;             // 线程返回值
    InfraxSync* mutex;        // 状态保护
} InfraxThread;
```

### 2. 错误码定义

```c
// 线程相关错误码
#define INFRAX_ERROR_THREAD_OK                0
#define INFRAX_ERROR_THREAD_INVALID_ARGUMENT -201
#define INFRAX_ERROR_THREAD_CREATE_FAILED    -202
#define INFRAX_ERROR_THREAD_JOIN_FAILED      -203
#define INFRAX_ERROR_THREAD_ALREADY_RUNNING  -204
#define INFRAX_ERROR_THREAD_NOT_RUNNING      -205
```

### 3. 线程池实现步骤

1. 基础设施准备
   - 完善错误码定义
   - 实现线程配置结构
   - 实现线程结构

2. 同步原语封装
   - 使用 InfraxSync 替换直接的 pthread 调用
   - 实现同步原语的生命周期管理

3. 线程池核心实现
   - 实现工作线程函数
   - 实现任务队列管理
   - 实现线程池状态管理

4. 内存管理改进
   - 使用 InfraxCore 的内存管理接口
   - 完善内存分配失败处理

5. 错误处理完善
   - 统一使用 make_error 创建错误
   - 完善错误传递机制

## 执行计划

1. 第一阶段：基础结构调整
   - 修改头文件定义
   - 实现基本的线程操作

2. 第二阶段：同步机制完善
   - 实现同步原语封装
   - 完善线程状态管理

3. 第三阶段：线程池实现
   - 实现线程池核心功能
   - 实现任务调度机制

4. 第四阶段：健壮性增强
   - 完善错误处理
   - 增加调试支持
   - 添加性能统计

## 注意事项

1. 保持向后兼容性
2. 确保线程安全
3. 避免资源泄漏
4. 保持代码风格一致
5. 添加必要的注释和文档 