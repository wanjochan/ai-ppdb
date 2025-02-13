# OOP-like infrastructure for PPDB

```
InfraxCore (核心基础设施)
├── 基础功能
│   ├── 字符串操作
│   ├── 时间管理
│   ├── 随机数
│   └── 网络字节序转换
├── 数据结构
│   ├── Buffer
│   ├── RingBuffer
│   └── 文件操作
│
├── InfraxMemory (内存管理)
│
├── InfraxSync (同步原语)
│   └── InfraxMemory
│
├── InfraxThread (线程管理)
│   ├── InfraxSync
│   └── InfraxMemory
│
├── InfraxNet (网络操作)
│
├── InfraxLog (日志系统)
│
└── InfraxAsync (异步操作)
    └── InfraxThread
        ├── InfraxSync
        └── InfraxMemory

设计模式:
├── 单例模式: InfraxCore, InfraxLog
├── 工厂模式: InfraxNet, InfraxSync
└── OOP风格: 所有模块都采用仿面向对象设计
```