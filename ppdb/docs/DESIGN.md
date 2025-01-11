# PPDB 架构设计

## 项目目标

构建一个高性能的分布式数据库，目前简化的目标是：
- 内存KV存储（兼容Memcached）
- 持久化存储
- 分布式集群

## 特别注意

使用 cosmopolitan 跨平台底层，所以全部标准c的头文件都不需要引用

## 目录结构

```
src/
├── infra/                     # 基础设施层
│   ├── infra_{module}.c          
│
├── memkv/                     # 内存KV层
│   ├── memkv.c               # memkv主入口（被 libppdb.c 调用）
│   ├── memkv_store.c         # 内存存储实现
│   └── memkv_peer.c          # memcached协议服务
│
├── diskv/                     # 持久化层
│   ├── diskv.c               # diskv主入口（被 libppdb.c 调用）
│   ├── diskv_store.c         # 磁盘存储实现（含WAL）
│   └── diskv_peer.c          # 存储服务协议
│
└── ppdb/                      # 产品层
    ├── ppdb.c                # 服务端主程序
    └── libppdb.c             # 客户端库 （被 ppdb.c 调用）
```

## 头文件结构

```
include/                       # 公共头文件目录
└── ppdb.h                    # 唯一对外头文件

src/internal/                 # 内部头文件目录
├── infra/                    # 基础设施头文件
├── memkv/                    # 内存KV头文件
├── diskv/                    # 持久化头文件
└── ppdb/                     # 产品头文件
```

## 开发阶段

0. **基础设施层**

特别注意：这一层不要出现 ppdb_ 字眼，这是基础设施层infra

layer：infra

所在目录：ppdb/src/internal/{layer}/
{layer}.h: 合并头文件（等infra层完全稳定我们是要打包出ppdbinfra这个静态库和动态库，所以到时移动到ppdb.h同一个目录）

模块文件命名模式：{layer}_{module}.h 和 {layer}_{module}.c

infra层的模块分类：
core: 基础功能
platform: 平台抽象（在cosmopolitan已经封装好绝大部分的基础上再稍微消除一些平台差异）
memory: 内存管理
error: 错误处理
struct: 数据结构
sync: 同步（互斥、锁、条件变量、信号量、无锁等）
async.h: 异步（暂时遇到设计瓶颈，考虑先只封装基本的，比如epoll? iocp?）
peer: 实例、进程、网络等

1. **第一阶段：MemKV**
   - 实现基础设施层
   - 实现内存KV存储
   - 支持Memcached协议

2. **第二阶段：DiskV**
   - 实现持久化存储
   - 添加WAL日志
   - 实现数据恢复

3. **第三阶段：集群**
   - 实现分布式协议
   - 支持数据复制
   - 实现一致性保证

## 同步/异步设计

1. **同步操作**
   - 内存操作
   - 本地计算
   - 简单查询

2. **异步操作**
   - 网络IO
   - 磁盘IO
   - 定时任务

## 测试结构

采用 mock 机制进行单元测试:

mock 机制通过替换真实函数调用来模拟组件行为。主要用于:
- 隔离外部依赖(文件系统、网络等)
- 模拟错误情况
- 验证函数调用是否符合预期

框架提供 MOCK_FUNC() 定义 mock 函数,通过 mock_register_expectation() 设置预期行为。目前已实现了内存管理、平台抽象等模块的 mock。

```

test/white/
├── framework/              # 测试框架
│   ├── test_framework.h   # 测试框架
│   ├── mock_framework.h   # mock框架
│   └── mock_framework.c
│
└── infra/                 # infra层的mock实现
    ├── mock_memory.h      # 内存管理mock
    ├── mock_memory.c
    ├── mock_platform.h    # 平台抽象mock
    └── mock_platform.c

```

测试顺序：
.\pdpb\scripts\build_test42.bat 用于确认 cross9/cosmopolitan 工具链运作正常（如果不正常就停下讨论）
.\ppdb\scripts\build_test_mock.bat 用于确定 mock 机制运作正常
.\ppdb\scripts\build_test_infra.bat 用于确定 infra 层运作正常
.\ppdb\scripts\build_ppdb.bat 构建 libppdb.a 和 ppdb.exe（以后可能还会生成 ppdb.lib作为跨平台动态库）
