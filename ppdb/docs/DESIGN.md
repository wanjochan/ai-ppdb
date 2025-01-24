# PPDB 架构设计

## 项目目标

分布式数据库（分阶段实现）：
- MemKV （开发中，兼容Memcached基本协议）
- DisKV （持久化存储，兼容 Redis基本协议）
- Distributed Cluster 分布式集群

## 目录结构

```

以下指【项目根目录/ppdb】：
* 命名模式：src/internal/{layer}/{layer}_{module}.[ch]

src/
│  ├──/internal/
│     ├── infra/                 # 基础设施层（基于cosmopolitan底层），未经许可不可修改 infra 层任何代码！！！
│     │   ├── infra_{module}.[ch]         
│     │
│     ├── poly/                  # 工具组件 （调用 infra层，不允许调用cosmopolitan或libc）
│     │   ├── poly_{module}.[ch]
│     │
│     ├── peer/                  # 产品组件 （调用 工具组件 和 infra 层，不允许调用cosmopolitan或libc）
│     │   ├── peer_{module}.[ch]
│      
└── ppdb/                  # 产品层（调用 产品组件 和 工具组件 和 infra层，不允许调用cosmopolitan或libc）
    ├── ppdb.c             # 服务端主程序 ppdb.exe 能跨平台运作
    └── libppdb.c          # libppdb.a（静态库） and libppdb.lib（动态库）

include/                   # 公共头文件目录
└── ppdb.h                    # 唯一对外头文件

```

## 分层细节展开

I 基础设施层 Infra 模块

core: 核心基础或未分类模块功能
platform: 平台抽象（跨平台基本在cosmopolitan已经封装好，但会稍微有些调整来细微的平台差异）
memory: 内存管理（含系统模式和内存池模式）
error: 错误处理
ds: 基本数据结构
sync: 同步（互斥、锁、条件变量、信号量、无锁lockfree、线程池等）
mux：多路复用（windows自动使用IOCP，linux使用epoll）
net：网络

II Poly 可重用工具组件层

brev.

III 产品层 Peers

. 客户端工具
fetcher (wget+curl+fetch)
cdper (cdp web client helper)
proxier (proxy client helper)

.Rinetd

网络转发服务器（平替 rinetd），顺便测试infra层的多路复用和网络模块

. MemKV
   - 实现内存 KV 存储
   - 兼容 Memcached 协议
   - 兼容 ppdb binary 流数据协议（待设计）

. DiskV
   - 实现持久化存储
   - 自带 WAL 日志且支持数据自恢复
   - 兼容 Redis 协议
   - 兼容 ppdb binary 流数据协议（待设计）

. 集群
   - 实现分布式部署
   - 实现最终一致性保证（暂不支持交易属性）

- 其他
   - 计划实现 IPFS 星际协议
   - 计划支持 mysql 协议
   - 计划支持 GraphQL 查询
   - 计划支持自然语言模糊查询

## 测试结构

- 采用 mock 机制进行单元测试，通过替换真实函数调用来模拟组件行为。主要用于:
- 隔离外部依赖(文件系统、网络等)
- 模拟错误情况
- 验证函数调用是否符合预期

框架提供 MOCK_FUNC() 定义 mock 函数,通过 mock_register_expectation() 设置预期行为。目前已实现了内存管理、平台抽象等模块的 mock。

- 单元测试结构

```
test/white/
├── framework/             # 测试框架
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

- 测试流程

```
# 【热身】主要用于确认 cross9/cosmopolitan 工具链运作正常，运行后不管是否正常都算热身完毕，停下来等待下一步安排。
.\pdpb\scripts\build_test42.bat

# 用于确定 mock 机制运作正常
.\ppdb\scripts\build_test_mock.bat  

# 用于确定 infra 层运作正常
.\ppdb\scripts\build_test_infra_all.bat 全部infra模块
.\ppdb\scripts\build_test_infra.bat [module] [norun]  //用于确定 infra 层运作正常
  - 不带参数：会触发帮助
  - module参数：指定要测试的模块，当前支持的测试模块包括：
      - memory：内存管理测试
      - log：日志功能测试
      - sync：同步机制测试
      - error：错误处理测试
      - struct：数据结构测试
      - memory_pool：内存池测试
  - norun参数：只构建不运行测试

# 构建 ppdb 产品
.\ppdb\scripts\build_ppdb.bat 构建 libppdb.a 和 ppdb.exe（以后可能还会生成 ppdb.lib作为跨平台动态库）
  里面会复制 ppdb.exe 到 .\ppdb\ppdb_latest.exe

  .\ppdb\scripts\build_ppdb.bat

## rinetd
  .\ppdb\ppdb_latest.exe --log-level=5 rinetd --start
  .\ppdb\ppdb_latest.exe --log-level=5 rinetd --config ./ppdb/rinetd2.conf --start


##tccrun (paused dev)
.\ppdb\ppdb_latest.exe --log-level=4 tccrun --source .\ppdb\test2.c -I.\repos\cosmopolitan_pub -L .\repos\cosmopolitan_pub -lcosmopolitan.a
.\ppdb\ppdb_latest.exe --log-level=1 tccrun --source ppdb/test2.c -I repos\cosmopolitan_pub -L repos\cosmopolitan_pub -l cosmopolitan.a
.\ppdb\ppdb_latest.exe --log-level=5 tccrun --source .\ppdb\test.c

## memkv

./ppdb/ppdb_latest.exe --log-level=5 memkv --start

```