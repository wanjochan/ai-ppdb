# PPDB 架构设计

## 项目目标

分布式数据库（分阶段实现）：
- MemKV （开发中，兼容Memcached基本协议）
- DisKV （持久化存储，兼容 Redis基本协议）
- Distributed Cluster 分布式集群

## 目录结构与架构演进

### 传统 PPDB 架构（ppdb/）
```
src/
│  ├──/internal/
│     ├── infra/                 # 基础设施层
│     ├── poly/                  # 工具组件层
│     ├── peer/                  # 产品组件层
└── ppdb/                        # 产品层
```

### 新架构 PPX（ppx/）：面向对象的模块化设计

```
ppx/
│  ├──/src/internal/
│     ├── infrax/                # 基础设施层（跨平台支持）
│     │   ├── InfraxCore         # 核心功能模块
│     │   ├── InfraxMemory       # 内存管理
│     │   ├── InfraxAsync        # 异步事件处理
│     │   └── InfraxNet          # 网络抽象
│     │
│     ├── polyx/                 # 工具组件层
│     │   ├── PolyxAsync         # 高级异步事件管理
│     │   ├── PolyxCmdline       # 命令行接口
│     │   └── PolyxService       # 服务管理
│     │
│     ├── peerx/                 # 服务层
│     │   ├── PeerxService       # 服务基类
│     │   ├── PeerxRinetd        # 网络转发服务
│     │   └── PeerxMemKV         # 内存 KV 存储服务
│     │
│     └── arch/                  # 架构工具
│         └── PpxArch.h          # 架构定义
```

## 架构设计原则

### 1. 分层解耦
- **Infrax 层**：提供底层系统抽象
  - 跨平台支持（基于 Cosmopolitan）
  - 内存管理
  - 错误处理
  - 同步原语
  - 网络基础设施

- **Polyx 层**：构建可重用工具组件
  - 异步事件处理
  - 命令行接口
  - 服务管理
  - 仅允许调用 Infrax 层接口

- **Peerx 层**：具体服务实现
  - 网络转发服务
  - 内存 KV 存储
  - 持久化存储
  - 调用 Polyx 和 Infrax 层

### 2. 事件驱动设计
- 使用 `poll()` 多路复用机制
- 支持多种事件类型
  - I/O 事件
  - 定时器事件
  - 网络事件
  - 信号事件

### 3. 面向对象的 C 编程
- 通过结构体和函数指针模拟类
- 单例模式
- 方法继承和多态

### 4. 高度可扩展性
- 模块化设计
- 低耦合
- 支持动态服务注册

## 模块功能

### Infrax 层核心功能
- 内存管理（系统模式和内存池）
- 错误处理（支持堆栈跟踪）
- 同步原语
- 网络抽象
- 基本数据结构

### Polyx 层工具组件
- 异步事件管理
- 命令行接口
- 服务生命周期管理
- 调试和日志支持

### Peerx 层服务
- 网络转发服务（Rinetd）
- 内存 KV 存储（MemKV）
- 持久化存储（DiskV）
- 分布式集群（未来计划）

## 性能与可靠性

- 跨平台支持
- 低延迟事件处理
- 灵活的错误处理机制
- 高效的内存管理
- 可扩展的服务架构

## 未来路线图

- 实现 IPFS 星际协议支持
- 支持 MySQL 协议
- 支持 GraphQL 查询
- 自然语言模糊查询
- 分布式一致性优化

## 测试策略

- 单元测试覆盖每个模块
- 集成测试验证层间交互
- 性能基准测试
- 故障注入测试

## 开发建议

- 严格遵守分层依赖原则
- 保持模块低耦合
- 详细的文档和注释
- 持续重构和优化