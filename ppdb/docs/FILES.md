# PPDB 源码索引

## 构建脚本 (scripts/)
- `build.bat` - Windows 平台构建脚本
  * 参数格式：`build.bat [target] [mode]`
  * target: 构建目标（如 base, engine, storage, all 等）
  * mode: debug 或 release
  * 示例：`build.bat store debug`

## 核心源代码 (src/)

### 基础模块 (src/base/)
- `base.c` - 基础功能入口
- `base_counter.inc.c` - 计数器实现
- `base_error.inc.c` - 错误处理
- `base_memory.inc.c` - 内存管理
- `base_skiplist.inc.c` - 跳表实现
- `base_spinlock.inc.c` - 自旋锁实现
- `base_struct.inc.c` - 基础数据结构
- `base_sync.inc.c` - 同步原语
- `base_utils.inc.c` - 工具函数

### 引擎模块 (src/engine/)
- `engine.c` - 引擎功能入口
- `engine_core.inc.c` - 核心功能
- `engine_error.inc.c` - 错误处理
- `engine_io.inc.c` - IO操作
- `engine_struct.inc.c` - 引擎数据结构
- `engine_txn.inc.c` - 事务处理

### 存储模块 (src/storage/)
- `storage.c` - 存储功能入口
- `skiplist_lockfree.inc.c` - 无锁跳表实现
- `storage_index.inc.c` - 索引管理
- `storage_ops.inc.c` - 存储操作
- `storage_table.inc.c` - 表管理

### 节点通信模块 (src/peer/)
- `peer.c` - 节点管理
- `peer_connection.inc.c` - 连接管理

### 内部头文件 (src/internal/)
- `base.h` - 基础模块头文件
- `engine.h` - 引擎模块头文件
- `storage.h` - 存储模块头文件

## 公共头文件 (include/ppdb/)
- `ppdb.h` - 主头文件，包含基础定义

## 文件依赖关系

### 基础模块
```
ppdb/ppdb.h
└── src/internal/base.h
    └── src/base/*.inc.c
```

### 引擎模块
```
ppdb/ppdb.h
└── src/internal/engine.h
    └── src/engine/*.inc.c
```

### 存储模块
```
ppdb/ppdb.h
└── src/internal/storage.h
    └── src/storage/*.inc.c
```

## 代码规范

### 1. 目录结构
- 公共接口：`include/ppdb/`
- 内部实现：`src/`
- 内部头文件：`src/internal/`
- 测试代码：`test/`

### 2. 命名规范
- 公共接口前缀：`ppdb_`
- 内部接口前缀：根据模块定义（如 `base_`, `engine_`, `storage_`）
- 测试文件前缀：`test_`

### 3. 头文件包含
- 优先使用项目根目录的相对路径
- 公共头文件使用 `<>` 包含
- 内部头文件使用 `""` 包含

### 4. 文件命名规范
- `.h` - 头文件，定义接口和数据结构
- `.c` - 主要实现文件
- `.inc.c` - 内部实现文件，通过主实现文件包含
- `.del` - 已删除/废弃的文件

### 5. 注释规范
- 文件头部：文件说明、作者、日期
- 函数：功能说明、参数、返回值
- 复杂逻辑：实现说明、注意事项
- 中文注释：使用 UTF-8 编码 