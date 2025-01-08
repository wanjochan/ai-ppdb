# PPDB文件结构说明

## 源代码目录 (src/)

### 基础模块
- `base.c` - 基础功能入口
- `base_error.inc.c` - 错误处理
- `base_list.inc.c` - 链表实现
- `base_map.inc.c` - 映射实现
- `base_mem.inc.c` - 内存管理
- `base_mutex.inc.c` - 互斥锁
- `base_skiplist.inc.c` - 跳表实现
- `base_string.inc.c` - 字符串处理
- `base_thread.inc.c` - 线程管理

### 数据库模块
- `database.c` - 数据库功能入口
- `database_core.inc.c` - 核心功能实现
- `database_txn.inc.c` - 事务管理
- `database_mvcc.inc.c` - MVCC实现
- `database_memkv.inc.c` - 内存存储
- `database_index.inc.c` - 索引管理

### 网络模块
- `peer.c` - 网络功能入口
- `peer_conn.inc.c` - 连接管理
- `peer_proto.inc.c` - 协议实现
- `peer_server.inc.c` - 服务器实现
- `peer_memcached.inc.c` - Memcached协议
- `peer_redis.inc.c` - Redis协议

### 头文件 (src/internal/)
- `base.h` - 基础模块头文件
- `database.h` - 数据库模块头文件
- `peer.h` - 网络模块头文件

## 测试代码目录 (test/)

### 白盒测试 (test/white/)
- `base/` - 基础模块测试
- `database/` - 数据库模块测试
- `peer/` - 网络模块测试

### 黑盒测试 (test/black/)
- `api/` - API接口测试
- `perf/` - 性能测试
- `stress/` - 压力测试

## 构建脚本目录 (scripts/)
- `build.bat` - 主构建脚本
- `build_env.bat` - 环境变量设置
- `build_base.bat` - 基础模块构建
- `build_database.bat` - 数据库模块构建
- `build_peer.bat` - 网络模块构建
- `build_test.bat` - 测试代码构建

## 文档目录 (docs/)
- `API.md` - API文档
- `BUILD.md` - 构建说明
- `DESIGN.md` - 设计文档
- `FILES.md` - 本文件
- `README.md` - 项目说明