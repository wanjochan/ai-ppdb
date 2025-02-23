# PPDB 架构设计文档

## 1. 概述

PPDB 是一个分布式数据库系统的概念验证(PoC)实现。系统目标是提供高性能、可扩展的数据存储和处理能力。

### 1.1 设计目标

- 高性能：支持并行处理
- 可扩展：支持多种存储引擎
- 可靠性：提供事务支持
- 跨平台：基于 cosmopolitan 实现

### 1.2 核心特性

- 多存储引擎支持（SQLite、DuckDB）
- 键值存储接口
- 脚本扩展系统

## 2. 系统架构

PPDB 采用模块化设计，主要包含以下组件：

### 2.1 基础设施

- 内存管理
- 错误处理
- 数据结构（哈希表、红黑树等）
- 缓冲区操作
- 跨平台支持

### 2.2 数据库抽象

- 统一的数据库操作接口
- 事务管理
- 查询处理

### 2.3 网络服务

- 基本的网络通信
- 服务管理
- 数据同步

## 3. 核心组件

### 3.1 数据库引擎

#### 3.1.1 SQLite 引擎
- 内置支持
- 适用于单机场景
- 提供完整的 SQL 支持

#### 3.1.2 DuckDB 引擎
- 动态加载支持
- 面向分析的列式存储
- 高性能并行处理

### 3.2 存储引擎

#### 3.2.1 MemKV
- 内存键值存储
- 高性能读写
- 支持数据过期
- 原子操作支持

### 3.3 脚本系统

- 支持自定义函数
- 错误处理机制
- 类型系统
- 环境隔离

## 4. 关键接口

### 4.1 数据库接口

基本操作：
```c
InfraxError (*open)(PolyxDB* self, const polyx_db_config_t* config);
InfraxError (*close)(PolyxDB* self);
InfraxError (*exec)(PolyxDB* self, const char* sql);
InfraxError (*query)(PolyxDB* self, const char* sql, polyx_db_result_t* result);
```

事务支持：
```c
InfraxError (*begin)(PolyxDB* self);
InfraxError (*commit)(PolyxDB* self);
InfraxError (*rollback)(PolyxDB* self);
```

KV操作：
```c
InfraxError (*set)(PolyxDB* self, const char* key, const void* value, size_t value_size);
InfraxError (*get)(PolyxDB* self, const char* key, void** value, size_t* value_size);
InfraxError (*del)(PolyxDB* self, const char* key);
```

### 4.2 服务接口

服务生命周期管理：
- 服务注册
- 状态管理
- 配置应用
- 启动/停止控制

## 5. 开发状态

### 5.1 当前状态
- 已完成概念验证(PoC)
- 代码将迁移到新的 PPX 架构
- 具有基本的测试框架

### 5.2 后续计划
- 迁移到 PPX 架构
- 完善功能
- 优化性能
- 完善文档

## 6. 优势与改进空间

### 6.1 主要优势
- 模块化设计
- 多存储引擎支持
- 基本的事务支持
- 跨平台能力

### 6.2 改进空间
- 完善分布式功能
- 增强并行处理能力
- 优化内存使用
- 增加更多存储引擎支持

### 注意
ppdb 已经基本完成 PoC，目前正在重构为 PPX 架构（INFRAX, POLYX, PEERX），预计完成迁移后会删除 ppdb 目录。

### test arch (infrax+polyx+peerx)
rm -rf ppx/build/ && timeout 60s sh ppx/scripts/build_test_arch.sh

测试 sqlite3 
服务端：sh ppdb/scripts/build_ppdb.sh && ./ppdb/ppdb_latest.exe sqlite3 --start --config=ppdb/sqlite3.conf --log-level=5
客户端：${HOME}/miniconda3/bin/python3 ppdb/test/black/system/test_sqlite3_service.py

# 严格按以下顺序进行 memkv 测试
  - 如果测试出现问题，不要着急改，要深度分析，分析出问题的原因，再根据原因进行修改 peer_memkv.c 文件
  - 如果改动了 peer_memkv.c 哪怕一点点也必须要严格按这个顺序重新来测试!
  - 如果编译不通过就恢复 peer_memkv.c 文件，重新再来!
  - 如果修复了任何一个测试，也要重头再来按顺序测试（因为有一定的依赖关系）

## 先是分步测试
sh ppdb/scripts/test_memkv.sh -k test_basic_set_get
sh ppdb/scripts/test_memkv.sh -k test_delete
sh ppdb/scripts/test_memkv.sh -k test_not_found
sh ppdb/scripts/test_memkv.sh -k test_multi_set_get
sh ppdb/scripts/test_memkv.sh -k test_expiration
sh ppdb/scripts/test_memkv.sh -k test_increment_decrement
sh ppdb/scripts/test_memkv.sh -k test_large_value
## 最后整体测试（目前未通过，要小心分析）：
sh ppdb/scripts/test_memkv.sh

#sh ppdb/scripts/test_memkv.sh -k test_flags

```

## 整体架构

PPDB采用分层架构设计，自下而上分为三层:


### 1. Infra 层 (基础设施层)
- 基于 cosmopolitan 实现跨平台
- 核心底层能模块:
  - 日志模块
  - 核心函数（字符串、Buffer、RingBuffer、随机数、时间、网络字节序、内存管理、错误处理）
  - 内存管理(系统模式、内存池、GC模式)
  - 同步原语(互斥、锁、条件变量等)
  - 网络（socket）
  - 线程（线程池）
  - 异步原语（定时器、异步任务）

### 2. Poly层 (工具组件层)
- 基于 Infra 层构建的可重用工具组件
- 限制:
  - 应只能调用Infra层接口，如果不够用应向 Infra 层添加（需经审批）
  - 原则上不允许直接调用cosmopolitan或libc
- 目标（抽象与复用）:
  - 抽象通用功能组件
  - 提高代码复用性

### 3. Peer层 (产品组件层)
- 具体功能模块实现
- 当前规划:
  - Rinetd: 第一个功能模块，网络转发服务（参见同名软件）
  - Sqlite: 相当于 sqlite3 的网络版
  - MemKV: 内存KV存储，兼容Memcached协议
  - DisKV: 持久化存储，兼容Redis协议【未开始，但底层重用 MemKV 的底层，即 poly_db(vender=sqlite3|duckdb)】
  - 分布式集群支持【未开始】
- 限制:
  - 只能调用Poly层和Infra层接口
  - 不允许直接调用系统API
  - 不允许适配操作系统
  - 不允许擅自更改 infra 层代码
  - 只能使用我们自己的构建脚本，不使用 make/gmake/cmake 等构建工具

## 项目目标

1. 分布式数据库系统:
- 高性能内存KV存储
- 可靠的持久化存储
- 分布式集群部署
- 协议兼容性(Memcached/Redis)
- 其它计划中（兼容列数据库、时序数据，参考 clickhouse/dolphindb/kdb/duckdb 等产品功能和设计理念）

2. 跨平台支持（目前主要依赖 cosmopolitan 实现）:
- 统一的API接口
- 自动适配不同平台
- 最小化平台差异

3. 可扩展性:
- 模块化设计
- 清晰的分层架构
- 标准化的接口
- 超小脚本引擎（AST 解析、执行，以后可能会整合 JIT/IR/LLVM/WASM）

## Rinetd 产品模块

作为第一个功能模块，主要目标:
1. 验证基础架构的可用性
2. 测试Infra层的网络和多路复用功能
3. 提供基础的网络转发服务
4. 为后续模块开发积累经验

## Sqlite3 产品模块

## MemKV 产品模块

作为第二个功能模块，主要特点:
1. 分层设计:
   - peer_memkv: 网络服务层,实现 memcached 协议
   - poly_memkv: KV存储抽象层,提供统一接口
   - poly_plugin: 插件系统,支持动态加载存储引擎
   - poly_sqlite/duckdb: 存储引擎实现
   [duckdb c api](https://duckdb.org/docs/api/c/api.html)

2. 存储引擎策略:
   - SQLite作为内置引擎(静态链接)，而且 poly_sqlite 可重用于其它组件
   - DuckDB作为扩展引擎(动态加载)，成为 memkv/diskkv 等备选底层引擎

3. 插件化优势:
   - 存储引擎可独立升级
   - 便于扩展新引擎
   - 复用性强

## DisKV 产品模块

TODO

## 测试框架

1. 测试策略:
- 白盒和黑盒测试
- 性能测试、场景测试

2. 测试覆盖:
- 基础功能测试
- 并发压力测试
- 错误处理测试
- 性能基准测试

## 未来路线图

- 实现 IPFS 星际协议支持
- 支持 MySQL 协议
- 支持 GraphQL 查询
- 自然语言模糊查询
- 分布式一致性优化