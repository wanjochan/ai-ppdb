# PPDB 架构设计 （由AI Cursor自己总结的）

## 整体架构

PPDB采用分层架构设计，自下而上分为三层:

【【【 注意，ppdb 已经基本完成 PoC，
目前正在重构 PPX（INFRAX, POLYX, PEERX））

测试 sqlite3 
服务端：sh ppdb/scripts/build_ppdb.sh && ./ppdb/ppdb_latest.exe sqlite3 --start --config=ppdb/sqlite3.conf --log-level=5
客户端：${HOME}/miniconda3/bin/python3 ppdb/test/black/system/test_sqlite3_service.py

# 严格按以下顺序进行 memkv 测试
  - 如果测试出现问题，不要着急改，要深度分析，分析出问题的原因，再根据原因进行修改 peer_memkv.c 文件
  - 如果改动了 peer_memkv.c 哪怕一点点也必须要严格按这个顺序重新来测试!
  - 如果编译不通过就恢复 peer_memkv.c 文件，重新再来!
  - 如果修复了任何一个测试，也要重头再来按顺序测试（因为有一定的依赖关系）
```
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
```


】】】

### 1. Infra层 (基础设施层)
- 基于cosmopolitan提供跨平台支持
- 核心功能模块:
  - 内存管理(系统模式和内存池模式)
  - 错误处理
  - 同步原语(互斥、锁、条件变量等)
  - 网络
  - 基本数据结构

### 2. Poly层 (工具组件层)
- 基于Infra层构建的可重用工具组件
- 限制:
  - 只能调用Infra层接口
  - 原则上不允许直接调用cosmopolitan或libc
- 目标:
  - 抽象通用功能组件
  - 提高代码复用性

### 3. Peer层 (产品组件层)
- 具体功能模块实现
- 当前规划:
  - Rinetd: 第一个功能模块，网络转发服务（参见同名软件）【基本完成】
  - MemKV: 内存KV存储，兼容Memcached协议【初步通过，待压测和优化】
  - DiskKV: 持久化存储，兼容Redis协议【未开始，但底层重用 MemKV 的底层，即 poly_db(vender=sqlite3|duckdb)】
  - 分布式集群支持【未开始】
- 限制:
  - 只能调用Poly层和Infra层接口
  - 不允许直接调用系统API
  - 不允许适配操作系统
  - 不允许擅自更改 infra 层代码

## 项目目标

1. 分布式数据库系统:
- 高性能内存KV存储
- 可靠的持久化存储
- 分布式集群部署
- 协议兼容性(Memcached/Redis)
- 其它计划中（兼容列数据库、时序数据，参考 clickhouse/dolphindb/kdb/duckdb 等产品功能和设计理念）

2. 跨平台支持:
- 统一的API接口
- 自动适配不同平台
- 最小化平台差异

3. 可扩展性:
- 模块化设计
- 清晰的分层架构
- 标准化的接口

## Rinetd模块

作为第一个功能模块，主要目标:
1. 验证基础架构的可用性
2. 测试Infra层的网络和多路复用功能
3. 提供基础的网络转发服务
4. 为后续模块开发积累经验

## MemKV模块

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

## 测试框架

1. 测试策略:
- 提供白盒和黑盒测试
- 支持性能测试和场景测试

2. 测试覆盖:
- 基础功能测试
- 并发压力测试
- 错误处理测试
- 性能基准测试

## 架构优势

1. 层次清晰，职责分明
2. 良好的可扩展性和可维护性
3. 完善的测试支持
4. 强大的跨平台能力
5. 标准化的开发规范