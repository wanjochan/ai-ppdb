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
├── poly/                     # 工具组件
│   ├── poly_{module}.c
│
├── peer/                     # 产品组件
│   ├── peer_{module}.c
│
└── ppdb/                     # 产品层
    ├── ppdb.c                # 服务端主程序
    └── libppdb.c             # 客户端库 （被 ppdb.c 调用）
```

## 头文件结构

```
include/                       # 公共头文件目录
└── ppdb.h                    # 唯一对外头文件

src/internal/                 # 内部头文件目录
├── infra/                    # 基础设施头文件
├── poly/                    # 
├── peer/                    # 
└── ppdb/                     # 产品头文件
```

## 分层

所在目录：ppdb/src/internal/{layer}/
{layer}.h: 合并头文件
模块文件命名模式：{layer}_{module}.h 和 {layer}_{module}.c

I 基础设施层 Infra

（等infra层完全稳定我们是要打包出ppdbinfra这个静态库和动态库，所以到时移动到ppdb.h同一个目录）

infra层的模块分类：
core: 基础功能
platform: 平台抽象（在cosmopolitan已经封装好绝大部分的基础上再稍微消除一些平台差异）
memory: 内存管理
error: 错误处理
ds: 数据结构
sync: 同步（互斥、锁、条件变量、信号量、无锁lockfree、线程池等）
mux：多路复用
net：网络 (with cosmopolitan, it is IOCP in windows)

II Poly 可重用组件、工具层

III 产品层 Peers

. 客户端工具
fetcher (wget+curl+fetch)
cdper (cdp client helper)
proxier (proxy client helper)

.Rinetd

网络转发服务器（平替 rinetd），顺便测试infra层的多路复用和网络模块

. MemKV
   - 实现基础设施层
   - 实现内存KV存储
   - 支持Memcached协议

. DiskV
   - 实现持久化存储
   - 添加WAL日志
   - 实现数据恢复

. 集群
   - 实现分布式协议
   - 支持数据复制
   - 实现一致性保证

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

测试顺序（如果已经在仓库根目录就不用cd改变目录）：
.\pdpb\scripts\build_test42.bat  //用于确认 cross9/cosmopolitan 工具链运作正常（如果不正常就停下讨论）
.\ppdb\scripts\build_test_mock.bat  //用于确定 mock 机制运作正常
.\ppdb\scripts\build_test_infra.bat [module] [norun]  //用于确定 infra 层运作正常
  - 不带参数：会触发帮助
  - module参数：指定要测试的模块（如memory、log等）
  - norun参数：只构建不运行测试
.\ppdb\scripts\build_ppdb.bat 构建 libppdb.a 和 ppdb.exe（以后可能还会生成 ppdb.lib作为跨平台动态库）

## 测试模块
当前支持的测试模块包括：
- memory：内存管理测试
- log：日志功能测试
- sync：同步机制测试
- error：错误处理测试
- struct：数据结构测试
- memory_pool：内存池测试

使用示例：
```batch
# 运行日志测试
.\ppdb\scripts\build_test_infra.bat log

# 只测试内存管理模块
.\ppdb\scripts\build_test_infra.bat memory

# 运行同步测试（暂时屏蔽了线程池，晚些再打开测试）
.\ppdb\scripts\build_test_infra.bat sync

# 只构建不运行测试
.\ppdb\scripts\build_test_infra.bat memory norun
```
