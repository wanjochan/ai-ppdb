# Pollset 实现计划

## 问题分析

### 需求
1. 实现 pollset_add_fd 和 pollset_remove_fd 接口
2. 通过 poll() 实现多路复用fd轮询
3. 按顺序支持以下fd类型：
   - event (用于实现timer)
   - file (文件读写)
   - socket (为服务器accept做准备)

### 技术要点
1. poll() 机制
   - 可同时监控多个fd的状态变化
   - 支持读、写、异常事件
2. fd类型处理
   - pipe: 用于实现事件通知（替代eventfd）
   - 普通文件fd: 读写操作
   - socket fd: 网络操作
3. 回调机制
   - 需要支持不同类型fd的回调处理

## 实现方案

### 第一阶段：基础框架 ✓
1. [x] 在 InfraxAsync.h 中定义接口
   ```c
   int (*pollset_add_fd)(InfraxAsync* self, int fd, short events, PollCallback cb, void* arg);
   int (*pollset_remove_fd)(InfraxAsync* self, int fd);
   int (*pollset_poll)(InfraxAsync* self, int timeout_ms);
   ```

2. [x] 实现 pollset 内部结构
   - [x] fd数组
   - [x] 事件数组
   - [x] 回调函数数组
   - [x] 用户数据数组

3. [x] 实现基础的 pollset_add_fd 和 pollset_remove_fd
   - [x] 动态扩容
   - [x] 重复fd处理
   - [x] 资源管理

### 第二阶段：Event实现 ✓
1. [x] 创建和管理事件通知机制
   - [x] 使用 pipe() 创建事件通道
   - [x] 实现事件触发机制
   - [x] 实现事件处理回调
2. [x] 实现基于event的timer
   - [x] 将现有timer改造为基于event
   - [x] 实现timer事件回调
   - [x] 支持周期性定时器
3. [ ] 配合线程池处理阻塞操作
   - [ ] 设计线程池接口
   - [ ] 实现任务分发机制

### 第三阶段：File实现 (待开始)
1. [ ] 文件读写fd的管理
2. [ ] 读写事件处理
3. [ ] 异步IO操作封装

### 第四阶段：Socket实现 (待开始)
1. [ ] socket fd管理
2. [ ] 连接事件处理
3. [ ] 数据收发处理

## 执行计划

### 当前任务
1. [x] 完善 InfraxAsync.h 中的接口定义
2. [x] 设计并实现内部pollset数据结构
3. [x] 实现基础的 pollset_add_fd 和 pollset_remove_fd
4. [x] 实现基于 pipe 的事件机制
5. [x] 基于事件机制实现 timer
6. [ ] 实现线程池支持

### 后续任务
1. [ ] File支持
2. [ ] Socket支持

## 注意事项
1. 需要考虑线程安全
2. 错误处理和资源清理
3. 性能优化
4. 跨平台兼容性

## 当前进展
- 已完成基础框架的实现，包括:
  1. pollset相关接口定义
  2. 内部数据结构设计
  3. 基础的fd管理功能
- 已完成基于 pipe 的事件机制实现:
  1. 事件创建和销毁
  2. 事件触发
  3. 事件处理回调
- 已完成基于事件的定时器实现:
  1. 定时器事件类型
  2. 定时器数据结构
  3. 周期性定时器支持
  4. 自动触发机制
- 下一步将实现线程池支持，用于处理阻塞操作

# Architecture Decision: Event and Timer Placement

## Decision
- Move Event and Timer systems from InfraxAsync to PolyxAsync
- Keep InfraxAsync focused on core async primitives

## Rationale
1. Separation of Concerns
   - InfraxAsync: coroutine + pollset + basic async primitives
   - PolyxAsync: event system + timer + higher-level async utilities

2. Benefits
   - Better modularity and maintainability
   - Clearer responsibility boundaries
   - Easier to extend and test
   - More flexible for future enhancements

## Implementation Plan
1. Clean up InfraxAsync
   - Remove Event/Timer related code
   - Focus on optimizing pollset implementation
   - Keep only essential async primitives

2. Enhance PolyxAsync
   - Implement Event system using InfraxAsync's pollset
   - Add Timer management
   - Consider adding more async utilities 