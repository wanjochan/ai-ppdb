# Polyx - High-level Polymorphic Extensions

```
PolyxAsync (高级异步操作)
├── 基础功能
│   ├── 文件操作 (read_file/write_file)
│   ├── HTTP操作 (http_get/http_post)
│   ├── 定时器 (delay/interval)
│   └── 任务编排 (parallel/sequence)
├── 依赖模块
│   ├── InfraxCore (基础设施)
│   ├── InfraxAsync (异步原语)
│   ├── InfraxMemory (内存管理)
│   └── InfraxNet (网络操作)

PolyDS (高级数据结构) [规划中]
├── 基础数据结构
│   ├── List (链表)
│   ├── Hash (哈希表)
│   ├── RBTree (红黑树)
│   ├── Buffer (缓冲区)
│   └── RingBuffer (环形缓冲区)
└── 依赖模块
    ├── InfraxCore
    └── InfraxMemory

设计特点:
├── 高级封装: 在 Infrax 基础上提供更友好的接口
├── 多态支持: 支持泛型和多态操作
├── 异步优先: 全面支持异步操作
└── 内存安全: 自动管理资源生命周期

设计模式:
├── 工厂模式: PolyxAsync
└── OOP风格: 所有模块都采用仿面向对象设计
