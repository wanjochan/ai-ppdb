# PPDB 源码索引

## 构建脚本 (scripts/)
- `build_ppdb.bat` - 主程序构建脚本，负责编译核心模块并生成可执行文件
- `build_test.bat` - 测试程序构建脚本，复用主构建配置编译测试用例

## 核心源代码 (src/)

### 主程序
- `src/main.c` - 程序入口，包含配置初始化和主循环

### KVStore 模块 (src/kvstore/)
- `kvstore.c` - KVStore 核心实现，管理存储引擎的主要功能
- `memtable.c` - 内存表基础实现
- `sharded_memtable.c` - 分片内存表实现，用于提高并发性能
- `skiplist.c` - 无锁跳表实现，作为内存表的基础数据结构
- `sync.c` - 同步原语实现，提供并发控制机制
- `wal.c` - Write-Ahead Log 实现，保证数据持久性
- `monitor.c` - 监控系统实现，收集性能指标
- `metrics.c` - 性能指标统计实现

## 头文件 (include/ppdb/)

### 公共接口
- `ppdb_error.h` - 错误码定义和错误处理接口
- `ppdb_kvstore.h` - KVStore 公共接口定义，提供所有 KV 存储操作

## 存储引擎接口
- `memtable.h` - 内存表接口定义
- `sharded_memtable.h` - 分片内存表接口定义
- `wal.h` - WAL 接口定义
- `fs.h` - 文件系统操作接口

## 内部头文件 (src/kvstore/internal/)
- `kvstore_internal.h` - KVStore 内部数据结构和函数定义
- `kvstore_types.h` - 基础类型和常量定义
- `kvstore_fs.h` - 文件系统操作实现
- `kvstore_memtable.h` - 内存表实现
- `kvstore_sharded_memtable.h` - 分片内存表实现
- `kvstore_wal.h` - WAL 实现
- `kvstore_monitor.h` - 监控系统实现
- `kvstore_logger.h` - 日志系统实现
- `sync.h` - 同步原语实现，包含互斥锁和原子操作

## 文件关系说明

### 核心组件依赖关系
1. KVStore 核心
   ```
   ppdb_kvstore.h
   └── kvstore_internal.h
       ├── kvstore_types.h
       ├── sync.h
       ├── kvstore_memtable.h
       ├── kvstore_wal.h
       └── kvstore_monitor.h
   ```

2. 内存管理
   ```
   memtable.h
   ├── sync.h
   ├── skiplist.h
   └── sharded_memtable.h
   ```

3. 持久化
   ```
   wal.h
   ├── sync.h
   └── fs.h
   ```

### 通用组件依赖
- 所有组件依赖 `ppdb_error.h` 进行错误处理
- 所有组件依赖 `kvstore_types.h` 获取基础定义
- 所有组件依赖 `sync.h` 进行并发控制
- 所有组件使用 `kvstore_logger.h` 进行日志记录

## 代码规范
1. 公共接口放在 `include/ppdb/` 目录
2. 内部实现放在 `src/kvstore/` 目录
3. 内部头文件放在 `src/kvstore/internal/` 目录
4. 测试代码放在 `test/` 目录

## 待优化项
1. 部分内部头文件引用路径需要统一
2. 同步原语使用需要统一到 `ppdb_sync_t`
3. 部分接口定义需要完善（如 skiplist.h）
4. 部分实现需要补充文档说明

## 测试代码 (test/)

### 白盒测试 (test/white/)
- `test_framework.h/c` - 测试框架核心实现
- `test_plan.h` - 测试计划和用例定义

#### 核心组件测试
- `test_kvstore.c` - KVStore 功能测试
- `test_memtable.c` - 内存表功能测试
- `test_wal.c` - WAL 基础功能测试
- `test_wal_concurrent.c` - WAL 并发测试
- `test_metrics.c` - 性能指标测试
- `test_atomic_skiplist.c` - 无锁跳表测试

#### 功能测试
- `test_basic.c` - 基础功能测试
- `test_concurrent.c` - 并发操作测试
- `test_edge.c` - 边界条件测试
- `test_iterator.c` - 迭代器功能测试
- `test_resource.c` - 资源管理测试
- `test_recovery.c` - 恢复机制测试
- `test_stress.c` - 压力测试

#### 测试入口
- `test_main.c` - 主测试入口
- `test_kvstore_main.c` - KVStore 测试入口
- `test_memtable_main.c` - 内存表测试入口
- `test_wal_main.c` - WAL 测试入口
- `test_metrics_main.c` - 性能指标测试入口
- `test_resource_main.c` - 资源管理测试入口
- `test_recovery_main.c` - 恢复机制测试入口
- `test_stress_main.c` - 压力测试入口
- `test_wal_concurrent_main.c` - WAL 并发测试入口

### 黑盒测试 (test/black/)
- `scenarios/` - 场景测试用例
- `system/` - 系统测试用例
- `unit/` - 单元测试用例
- `integration/` - 集成测试用例
- `performance/` - 性能测试用例 