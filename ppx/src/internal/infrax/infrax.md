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


```

/**
ref
repos/nng/src/platform/posix/posix_pollq_poll.c
它的精妙之处在于：

唤醒机制：使用管道实现唤醒，避免忙等
事件批处理：一次 poll 多个文件描述符
零拷贝：直接操作文件描述符，无需数据拷贝
无锁设计：使用原子操作和事件通知，最小化锁的使用
高效的事件分发：直接调用回调函数

所以我们
主要改进点：

唤醒机制：添加管道实现高效的事件通知
事件队列：分离就绪和等待队列，提高事件处理效率
原子操作：使用原子操作更新事件状态，减少锁的使用
批处理：一次处理多个就绪事件
零拷贝：直接操作文件描述符，避免数据拷贝
这些改进将显著提升性能：

减少 CPU 使用率
降低延迟
提高吞吐量
优化资源利用

 */

### async
DESIGN NOTES

design pattern: factory
main idea: mux (poll + callback)

poll() 是一个多路复用 I/O 机制
可以同时监控多个文件描述符的状态变化
主要监控读、写、异常三种事件
可被 poll() 监控的主要 fd 类型:
a) 网络相关
TCP sockets
UDP sockets
Unix domain sockets
b) 标准 I/O
管道(pipes)
FIFO
终端设备(/dev/tty)
标准输入输出(stdin/stdout/stderr)
c) 其他
字符设备
事件fd (eventfd) linux
定时器fd (timerfd) linux
信号fd (signalfd)
inotify fd (文件系统事件监控)

```