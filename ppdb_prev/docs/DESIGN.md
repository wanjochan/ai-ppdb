# PPDB 架构设计

## 项目目标

构建一个高性能的分布式数据库，支持：
- 内存KV存储（兼容Memcached）
- 持久化存储
- 分布式集群

## 目录结构

```
src/
├── infra/                     # 基础设施层
│   ├── infra_core.c          # 内存分配、错误处理
│   ├── infra_struct.c        # 基础数据结构
│   ├── infra_sync.c          # 同步原语
│   ├── infra_async.c         # 异步框架
│   ├── infra_timer.c         # 定时器
│   ├── infra_event.c         # 事件循环
│   ├── infra_io.c            # 通用IO框架
│   ├── infra_store.c         # 通用存储接口
│   ├── infra_buffer.c        # 通用缓冲区管理
│   └── infra_peer.c          # 通用网络接口
│
├── memkv/                     # 内存KV层
│   ├── memkv.c               # memkv主入口
│   ├── memkv_store.c         # 内存存储实现
│   └── memkv_peer.c          # memcached协议服务
│
├── diskv/                     # 持久化层
│   ├── diskv.c               # diskv主入口
│   ├── diskv_store.c         # 磁盘存储实现（含WAL）
│   └── diskv_peer.c          # 存储服务协议
│
└── ppdb/                      # 产品层
    ├── ppdb.c                # 服务端主程序
    └── libppdb.c             # 客户端库
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

