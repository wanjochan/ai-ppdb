# PPDB 源码索引

## 构建脚本 (scripts/)
- `build.bat` - Windows 平台构建脚本：
  * 参数格式：`build.bat [target] [mode]`
  * target: 构建目标（如 sync_locked, sync_lockfree 等）
  * mode: debug 或 release
  * 示例：`build.bat sync_locked debug`

## 核心源代码 (src/)

### 同步原语 (src/sync/)
- `internal/internal_sync.h` - 基础同步原语内部接口
- `internal/internal_sync.c` - 基础同步原语实现
- `ppdb_sync.h` - 数据库同步层接口
- `ppdb_sync.c` - 数据库同步层实现

### 公共模块 (src/common/)
- `error.h` - 错误处理接口
- `error.c` - 错误处理实现

## 头文件 (include/ppdb/)
- `ppdb.h` - 主头文件，包含基础定义
- `ppdb_sync.h` - 同步原语公共接口
- `ppdb_error.h` - 错误处理公共接口

## 测试代码 (test/)

### 基础设施测试 (test/white/infra/)
- `test_sync.c` - 同步原语测试
  * 支持有锁和无锁两种模式：
    - `sync_locked` - 测试有锁版本
    - `sync_lockfree` - 测试无锁版本

### 测试入口
- `test_main.c` - 主测试入口
  * 初始化测试环境
  * 运行测试用例
  * 输出测试结果

## 文件依赖关系

### 同步原语
```
ppdb/ppdb_sync.h
├── sync/internal/internal_sync.h
└── ppdb/ppdb.h

sync/internal/internal_sync.h
└── ppdb/ppdb.h
```

### 错误处理
```
ppdb/ppdb_error.h
└── ppdb/ppdb.h

src/common/error.h
└── ppdb/ppdb_error.h
```

## 代码规范

### 1. 目录结构
- 公共接口：`include/ppdb/`
- 内部实现：`src/`
- 内部头文件：`src/*/internal/`
- 测试代码：`test/`

### 2. 命名规范
- 公共接口前缀：`ppdb_`
- 内部接口前缀：根据模块定义（如 `sync_`）
- 测试文件前缀：`test_`

### 3. 头文件包含
- 优先使用项目根目录的相对路径
- 公共头文件使用 `<>` 包含
- 内部头文件使用 `""` 包含

### 4. 注释规范
- 文件头部：文件说明、作者、日期
- 函数：功能说明、参数、返回值
- 复杂逻辑：实现说明、注意事项
- 中文注释：使用 UTF-8 编码 