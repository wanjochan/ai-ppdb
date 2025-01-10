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

特别注意：这一层不要出现 ppdb_ 字眼，这是通用设施层

ppdb/src/internal/infra/infra.h: 基础设施头文件
ppdb/src/infra/infra.c: 基础功能（内存、字符串、日志、基础数据结构等）

ppdb/src/internal/infra/infra_platform.h: 平台抽象头文件
ppdb/src/infra/infra_platform.c: 平台抽象（线程等）

ppdb/src/internal/infra/infra_async.h: 异步头文件
ppdb/src/infra/infra_async.c: 异步

ppdb/src/internal/infra/infra_sync.h: 同步头文件
ppdb/src/infra/infra_sync.c: 同步

ppdb/src/internal/infra/infra_peer.h: 实例头文件
ppdb/src/infra/infra_peer.c: 实例（进程、网络）

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

```
test/
├── infra/                    # 基础设施测试
├── memkv/                    # memkv测试
├── diskv/                    # diskv测试
└── ppdb/                     # 产品集成测试
```

## 重构指南

1. **代码迁移**
   ```
   当前代码 => 新位置
   base_core.inc.c    => infra/infra_core.c
   base_struct.inc.c  => infra/infra_struct.c
   base_sync.inc.c    => infra/infra_sync.c
   base_async.inc.c   => infra/infra_async.c
   base_timer.inc.c   => infra/infra_timer.c
   base_net.inc.c     => infra/infra_peer.c
   ```

2. **核心接口**
   ```c
   // infra层示例
   struct store_ops {
       int (*open)(void* ctx);
       int (*close)(void* ctx);
       int (*get)(void* ctx, const char* key, void** value);
       int (*set)(void* ctx, const char* key, const void* value);
   };

   // memkv层示例
   struct memkv_store {
       struct store_ops ops;  // 实现store接口
       void* ctx;            // 存储上下文
   };
   ```

3. **重构顺序**
   - infra层：core => struct => sync => async => peer
   - memkv层：store => peer
   - ppdb层：libppdb => ppdb

构建工具：
scripts\build_test42.bat 用于确认 cross9/cosmopolitan 工具链运作正常（如果不正常就停下讨论）
scripts\build_{layer}_{module}.bat 用于构建指定层和模块的代码 （这个还没确定，可能还要讨论？）
scripts\build_ppdb.bat 构建 libppdb.a 和 ppdb.exe（以后可能还会生成 ppdb.lib作为跨平台动态库）
