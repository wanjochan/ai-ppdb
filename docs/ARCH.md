# PPDB 架构设计 （由AI Cursor自己总结的）

## 整体架构

PPDB采用分层架构设计，自下而上分为三层:

【【【 注意，ppdb 已经基本完成 PoC，
目前正在重构 PPX（INFRAX, POLYX, PEERX））

测试 sqlite3 
服务端：sh ppdb/scripts/build_ppdb.sh && ./ppdb/ppdb_latest.exe sqlite3 --start --config=ppdb/sqlite3.conf --log-level=5
客户端：${HOME}/miniconda3/bin/python3 ppdb/test/black/system/test_sqlite3_service.py

测试 memkv
独立终端（backend）启动服务端：sh ./ppdb/scripts/build_ppdb.sh && (pkill -9 -f "ppdb_latest.exe memkv" || true) && ./ppdb/ppdb_latest.exe memkv --start --config=ppdb/memkv.conf --log-level=5

其中多键值测试（未成功）：${HOME}/miniconda3/bin/python3 -m unittest ppdb/test/black/system/test_memkv_protocol.py -k test_multi_set_get -v

如果是全部的客户端测试：${HOME}/miniconda3/bin/python3 ppdb/test/black/system/test_memkv_protocol.py

别急于运行，先分析相关代码，理解代码逻辑。

前后端开发测试的方法论：
1）单独编译查看有没有代码错误，有就先修复
2）如果没有编译问题，就启动服务端并且 2>&1 & | head -n 4096
3）稍等几秒等第 2 步的服务器已经启动
4）执行客户端测试并且 2>&1 & | head -n 4096
5) 执行 pkill，让第 2 步的服务器能杀掉退出不影响下次测试而且让你可以获得完整日志

运行（前后端一起运行测试流程）：
sh ./ppdb/scripts/build_ppdb.sh && (pkill -9 -f "ppdb_latest.exe memkv" || true) && sleep 1 && (timeout 10s ./ppdb/ppdb_latest.exe memkv --start --config=ppdb/memkv.conf --log-level=5 2>&1 | tee >(head -n 4096) &) && sleep 2 && (timeout 10s ${HOME}/miniconda3/bin/python3 -m unittest ppdb/test/black/system/test_memkv_protocol.py -k test_multi_set_get -v 2>&1 | tee >(head -n 4096) ) ; sleep 1 && pkill -9 -f "ppdb_latest.exe memkv"
等它完全运行后，根据输出分析问题解决问题

运行（前后端一起运行测试流程）：
sh ./ppdb/scripts/build_ppdb.sh && (pkill -9 -f "ppdb_latest.exe memkv" || true) && sleep 1 && (timeout 15s ./ppdb/ppdb_latest.exe memkv --start --config=ppdb/memkv.conf --log-level=5 2>&1 | tee >(head -n 4096) &) && sleep 2 && (timeout 15s ${HOME}/miniconda3/bin/python3 ppdb/test/black/system/test_memkv_protocol.py -k test_multi_set_get -v 2>&1 | tee >(head -n 4096) ) ; sleep 1 && pkill -9 -f "ppdb_latest.exe memkv"
等它完全运行后，你要使用思维链，发现问题、分析推理问题、指定计划、解决问题，

sh ./ppdb/scripts/build_ppdb.sh && (pkill -9 -f "ppdb_latest.exe memkv" || true) && sleep 1 && (timeout 15s ./ppdb/ppdb_latest.exe memkv --start --config=ppdb/memkv.conf --log-level=5 2>&1 | tee >(head -n 4096) &) && sleep 2 && (timeout 15s ${HOME}/miniconda3/bin/python3 ppdb/test/black/system/test_memkv_protocol.py -v 2>&1 | tee >(head -n 4096) ) ; sleep 1 && pkill -9 -f "ppdb_latest.exe memkv"


想加前缀（但失败了，有其它问题）
sh ./ppdb/scripts/build_ppdb.sh && \
(pkill -9 -f "ppdb_latest.exe memkv" || true) && \
sleep 1 && \
(timeout 30s ./ppdb/ppdb_latest.exe memkv --start --config=ppdb/memkv.conf --log-level=5 2>&1 | sed 's/^/S> /' | tee >(head -n 4096) &) && \
sleep 2 && \
(timeout 20s ${HOME}/miniconda3/bin/python3 -m unittest ppdb/test/black/system/test_memkv_protocol.py -k test_multi_set_get -v 2>&1 | sed 's/^/C> /' | tee >(head -n 4096)) ; \
sleep 1 && \
pkill -9 -f "ppdb_latest.exe memkv"

perl -e 'alarm 20; exec @ARGV' "cd /Users/wjc/ai-ppdb/ppx && ./scripts/build_test_arch.sh"

timeout 20s cd /Users/wjc/ai-ppdb/ppx && ./scripts/build_test_arch.sh

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