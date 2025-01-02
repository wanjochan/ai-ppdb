# PPDB 源码索引

## 构建脚本 (scripts/)
- `build.bat` - Windows 平台构建脚本，复用主构建配置编译测试用例：
  * 参数 ppdb 为编译主程序
  * 其它参数为编译测试用例
  * 支持通过环境变量设置代理
  * 生成的可执行文件后缀为 .exe
- `build.sh` - Unix 平台构建脚本：
  * 功能同 build.bat
  * 支持通过环境变量设置代理
  * 生成的可执行文件后缀：
    - macOS: .osx
    - Linux: .lnx
- `setup.bat` - Windows 平台环境初始化脚本：
  * 创建必要的目录结构
  * 下载和安装工具链（cosmocc、cross9）
  * 准备运行时文件
  * 克隆参考代码
  * 验证环境配置
  * 支持通过命令行参数或环境变量设置代理
- `setup.sh` - Unix 平台环境初始化脚本：
  * 功能同 setup.bat
  * 支持通过命令行参数或环境变量设置代理
  * 根据操作系统生成不同后缀的可执行文件

## 核心源代码 (src/)

### 主程序
- `src/main.c` - 程序入口，包含配置初始化和主循环

### 公共模块 (src/common/)
- `error.c` - 错误处理实现
- `fs.c` - 文件系统操作实现，提供统一的文件操作接口
- `logger.c` - 日志系统实现

### KVStore 模块 (src/kvstore/)
- `kvstore.c` - KVStore 核心实现，管理存储引擎的主要功能
- `kvstore_impl.c` - KVStore 内部实现，包含具体的存储操作实现
- `memtable.c` - 内存表基础实现
- `memtable_iterator.c` - 内存表迭代器实现
- `sharded_memtable.c` - 分片内存表实现，用于提高并发性能
- `skiplist.c` - 无锁跳表实现，作为内存表的基础数据结构
- `sync.c` - 同步原语实现，提供并发控制机制
- `wal.c` - Write-Ahead Log 实现，保证数据持久性
- `wal_write.c` - WAL 写入相关实现
- `wal_iterator.c` - WAL 迭代器实现
- `wal_maintenance.c` - WAL 维护相关实现
- `wal_recovery.c` - WAL 恢复相关实现
- `monitor.c` - 监控系统实现，收集性能指标
- `metrics.c` - 性能指标统计实现

## 头文件 (include/ppdb/)

### 公共接口
- `ppdb_error.h` - 错误码定义和错误处理接口
- `ppdb_types.h` - 基础类型定义，包含压缩、运行模式等公共类型
- `ppdb_kvstore.h` - KVStore 公共接口定义，提供所有 KV 存储操作
- `ppdb_fs.h` - 文件系统操作接口，提供统一的文件操作抽象
- `ppdb_wal.h` - WAL 公共接口定义，提供 WAL 相关操作

## 内部头文件 (src/kvstore/internal/)

### KVStore 核心实现
- `kvstore_internal.h` - KVStore 内部数据结构和函数定义
- `kvstore_types.h` - 内部类型定义（如键值对、迭代器等）
- `kvstore_config.h` - KVStore 内部配置扩展定义
- `kvstore_impl.h` - KVStore 实现的内部接口定义

### 存储引擎组件
- `kvstore_memtable.h` - 内存表实现
- `kvstore_sharded_memtable.h` - 分片内存表实现
- `kvstore_wal.h` - WAL 实现
- `kvstore_wal_types.h` - WAL 相关类型定义

### 辅助组件
- `kvstore_monitor.h` - 监控系统实现
- `kvstore_fs.h` - 文件系统操作的内部实现

### 基础设施
- `sync.h` - 同步原语实现，包含互斥锁和原子操作
- `metrics.h` - 性能指标定义
- `skiplist.h` - 无锁跳表实现

### 文件关系说明

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
   └── ppdb/ppdb_fs.h
   ```

4. 公共组件
   ```
   src/common/
   ├── error.c
   │   └── ppdb/ppdb_error.h
   ├── fs.c
   │   ├── ppdb/ppdb_fs.h
   │   └── kvstore/internal/kvstore_fs.h
   └── logger.c
       └── kvstore/internal/kvstore_logger.h
   ```

### 公共组件依赖
- 所有组件依赖 `ppdb_error.h` 进行错误处理
- 所有组件依赖 `ppdb_types.h` 获取基础类型定义
- 所有组件依赖 `sync.h` 进行并发控制
- 所有组件使用 `ppdb_logger.h` 进行日志记录

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

## 代码规范
1. 公共接口放在 `include/ppdb/` 目录
2. 内部实现放在 `src/kvstore/` 目录
3. 内部头文件放在 `src/kvstore/internal/` 目录
4. 测试代码放在 `test/` 目录

## 待优化项
1. 内部头文件引用路径需要统一
   - 统一使用从项目根目录开始的路径（如 `ppdb/...`）
   - 避免使用相对路径引用
   - 修复测试代码中的路径引用问题

2. 类型定义需要规范化
   - 公共类型定义移至 `ppdb_types.h`
   - 内部类型加上 `kvstore_` 前缀
   - 完善 `skiplist.h` 中的类型定义
   - 测试框架类型定义需要独立头文件

3. 接口命名需要统一
   - 内部接口统一加上 `kvstore_` 前缀
   - 公共接口统一使用 `ppdb_` 前缀
   - 保持函数命名风格一致
   - 测试用例命名规范化

4. 同步原语使用需要统一
   - 统一使用 `ppdb_sync_t`
   - 完善同步原语的文档说明
   - 规范化锁的使用方式

5. 文档补充
   - 补充接口使用说明
   - 添加实现细节文档
   - 完善错误处理说明
   - 更新测试文档结构

6. 测试框架优化
   - 将测试框架类型定义拆分到独立头文件
   - 错误注入机制独立成单独模块
   - 统一测试用例命名规范
   - 完善测试资源管理

## 测试代码 (test/)

### 测试框架
- `test_framework.h/c` - 测试框架核心实现
- `test_plan.h` - 测试计划和用例定义
- `README.md` - 测试说明文档

### 基础设施测试 (test/white/infra/)
- `test_sync.c` - 同步原语测试
  * 通过编译宏 `PPDB_SYNC_USE_LOCK` 控制测试模式
  * `sync_locked` - 测试有锁版本的同步原语
  * `sync_lockfree` - 测试无锁版本的同步原语
- `test_skiplist.c` - 无锁跳表测试
- `test_metrics.c` - 性能指标测试

### 存储引擎测试 (test/white/storage/)
- `test_fs.c` - 文件系统测试
- `test_memtable.c` - 内存表测试
- `test_wal.c` - WAL测试

### WAL专项测试 (test/white/wal/)
- `basic_test.c` - WAL基础功能测试
- `test_concurrent.c` - WAL并发测试

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
  - 负责初始化测试环境
  - 运行所有测试用例
  - 输出测试结果
- `test_kvstore_main.c` - KVStore测试入口
- `test_memtable_main.c` - 内存表测试入口
- `test_metrics_main.c` - 性能指标测试入口
- `test_recovery_main.c` - 恢复机制测试入口
- `test_resource_main.c` - 资源管理测试入口
- `test_stress_main.c` - 压力测试入口
- `test_wal_main.c` - WAL测试入口
- `test_wal_concurrent_main.c` - WAL并发测试入口

### 黑盒测试 (test/black/)
- `scenarios/` - 场景测试用例
- `system/` - 系统测试用例
- `unit/` - 单元测试用例
- `integration/test_system.c` - 系统集成测试
  * 完整工作流测试
  * 数据恢复测试
  * 并发操作测试
- `performance/benchmark.c` - 性能测试用例
  * Memtable性能测试（读写延迟和吞吐量）
  * WAL性能测试（同步/异步写入对比）
  * 压缩性能测试（启用/禁用压缩对比）
  * 批量操作测试（批量vs单个操作）
  * 并发性能测试（多线程扩展性）

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

### 文件系统实现
1. 公共文件系统接口 (`ppdb_fs.h`)
   ```
   ppdb/ppdb_fs.h
   ├── ppdb/ppdb_error.h
   └── ppdb/ppdb_types.h
   ```

2. KVStore 文件系统实现 (`kvstore_fs.h`)
   ```
   kvstore/internal/kvstore_fs.h
   ├── ppdb/ppdb_fs.h
   ├── ppdb/ppdb_error.h
   ├── ppdb/ppdb_types.h
   └── kvstore/internal/kvstore_types.h
   ```

3. 文件系统操作分层
   - 基础操作：目录和文件的基本操作（创建、删除、重命名等）
   - 读写操作：文件内容的读取和写入
   - 特定操作：WAL和SSTable的专用操作

4. 错误处理
   - 统一使用 `ppdb_error_t` 错误码
   - 参数验证：所有接口都检查参数有效性
   - IO错误：统一转换为 `PPDB_ERR_IO`

5. 实现规范
   - 公共接口：以 `ppdb_fs_` 为前缀
   - 内部接口：以 `kvstore_fs_` 为前缀
   - 错误处理：统一的错误码和日志记录
   - 资源管理：确保文件句柄正确关闭

### 组件职责

1. 公共文件系统 (`src/common/fs.c`)
   - 实现基础文件系统操作
   - 提供统一的错误处理
   - 确保跨平台兼容性
   - 管理文件系统资源

2. KVStore文件系统 (`kvstore_fs.h`)
   - 实现数据库特定的文件操作
   - 管理WAL和SSTable文件
   - 提供目录结构管理
   - 处理数据库文件命名和组织

### 使用规范

1. 文件操作
   - 使用 `ppdb_fs_` 接口进行基础操作
   - 使用 `kvstore_fs_` 接口进行数据库特定操作
   - 正确处理所有错误情况
   - 及时关闭文件句柄

2. 目录管理
   - 使用 `kvstore_fs_init_dirs` 初始化数据库目录
   - 使用 `kvstore_fs_cleanup_dirs` 清理数据库目录
   - 遵循数据库目录结构规范
   - 正确处理权限和路径问题

3. 错误处理
   - 检查所有返回值
   - 记录详细错误信息
   - 保持错误传播链
   - 提供有意义的错误信息 