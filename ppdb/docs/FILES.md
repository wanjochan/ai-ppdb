# PPDB 源码索引

## 构建脚本 (scripts/)
- `build_ppdb.bat` - 主程序构建脚本，负责编译核心模块并生成可执行文件
- `build_test.bat` - 测试程序构建脚本，复用主构建配置编译测试用例

## 核心源代码 (src/)

### 主程序
- `src/main.c` - 程序入口，包含配置初始化和主循环

### KVStore 模块 (src/kvstore/)
- `kvstore.c` - KVStore 核心实现，管理存储引擎的主要功能
- `kvstore_impl.c` - KVStore 内部实现，包含具体的存储操作实现
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
- `ppdb_types.h` - 基础类型定义，包含压缩、运行模式等公共类型
- `ppdb_kvstore.h` - KVStore 公共接口定义，提供所有 KV 存储操作

### 存储引擎接口
- `memtable.h` - 内存表接口定义
- `sharded_memtable.h` - 分片内存表接口定义
- `wal.h` - WAL 接口定义
- `fs.h` - 文件系统操作接口

## 内部头文件 (src/kvstore/internal/)

### KVStore 核心实现
- `kvstore_internal.h` - KVStore 内部数据结构和函数定义
- `kvstore_types.h` - 内部类型定义（如键值对、迭代器等）
- `kvstore_config.h` - KVStore 内部配置扩展定义

### 存储引擎组件
- `kvstore_memtable.h` - 内存表实现
- `kvstore_sharded_memtable.h` - 分片内存表实现
- `kvstore_wal.h` - WAL 实现
- `kvstore_fs.h` - 文件系统操作实现

### 辅助组件
- `kvstore_monitor.h` - 监控系统实现
- `kvstore_logger.h` - 日志系统实现

### 基础设施
- `sync.h` - 同步原语实现，包含互斥锁和原子操作
- `metrics.h` - 性能指标定义
- `skiplist.h` - 无锁跳表实现

### 实现说明
1. 命名规范：
   - KVStore 相关的内部实现以 `kvstore_` 开头
   - 基础设施组件保持简单命名
   - 公共类型定义以 `ppdb_` 开头

2. 组件分层：
   - 公共接口：提供给外部使用的稳定接口和类型定义
   - 核心实现：存储引擎的主要组件
   - 辅助组件：提供监控、日志等支持
   - 基础设施：提供底层机制支持

## 文件关系说明

### 核心组件依赖关系
1. KVStore 核心
   ```
   ppdb/ppdb_kvstore.h
   ├── ppdb/ppdb_types.h
   └── kvstore/internal/kvstore_internal.h
       ├── kvstore/internal/kvstore_types.h
       ├── kvstore/internal/sync.h
       ├── kvstore/internal/kvstore_memtable.h
       ├── kvstore/internal/kvstore_wal.h
       └── kvstore/internal/kvstore_monitor.h
   ```

2. 内存管理
   ```
   kvstore/internal/kvstore_memtable.h
   ├── ppdb/ppdb_types.h
   ├── ppdb/ppdb_error.h
   ├── kvstore/internal/kvstore_types.h
   ├── kvstore/internal/sync.h
   ├── kvstore/internal/skiplist.h
   └── kvstore/internal/kvstore_sharded_memtable.h
   ```

3. 持久化
   ```
   kvstore/internal/kvstore_wal.h
   ├── ppdb/ppdb_types.h
   ├── ppdb/ppdb_error.h
   ├── kvstore/internal/kvstore_types.h
   ├── kvstore/internal/sync.h
   └── kvstore/internal/kvstore_fs.h
   ```

### 通用组件依赖
- 所有组件依赖 `ppdb_error.h` 进行错误处理
- 所有组件依赖 `ppdb_types.h` 获取基础类型定义
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

### 测试框架
- `test_framework.h/c` - 测试框架核心实现
- `test_plan.h` - 测试计划和用例定义

### 基础设施测试 (test/white/infra/)
- `test_sync.c` - 同步原语测试
- `test_skiplist.c` - 无锁跳表测试
- `test_metrics.c` - 性能指标测试

### 存储引擎测试 (test/white/storage/)
- `test_fs.c` - 文件系统测试
- `test_memtable.c` - 内存表测试
- `test_wal.c` - WAL测试

### 功能测试 (test/white/)
- `test_basic.c` - 基础功能测试
- `test_concurrent.c` - 并发操作测试
- `test_edge.c` - 边界条件测试
- `test_iterator.c` - 迭代器功能测试
- `test_kvstore.c` - KVStore功能测试
- `test_resource.c` - 资源管理测试
- `test_recovery.c` - 恢复机制测试
- `test_stress.c` - 压力测试

### 测试入口
- `test_main.c` - 主测试入口
- `test_kvstore_main.c` - KVStore测试入口
- `test_memtable_main.c` - 内存表测试入口
- `test_wal_main.c` - WAL测试入口
- `test_metrics_main.c` - 性能指标测试入口
- `test_resource_main.c` - 资源管理测试入口
- `test_recovery_main.c` - 恢复机制测试入口
- `test_stress_main.c` - 压力测试入口

### 黑盒测试 (test/black/)
- `scenarios/` - 场景测试用例
- `system/` - 系统测试用例
- `unit/` - 单元测试用例
- `integration/` - 集成测试用例
- `performance/` - 性能测试用例

### 测试说明
1. 测试分层：
   - 基础设施测试：测试底层组件
   - 存储引擎测试：测试核心存储功能
   - 功能测试：测试整体功能和场景

2. 测试类型：
   - 单元测试：测试单个组件
   - 集成测试：测试组件间交互
   - 性能测试：测试性能指标
   - 压力测试：测试系统稳定性

3. 测试框架：
   - 使用统一的测试框架
   - 支持断言和性能测试
   - 提供测试计划管理 