# AST 查询语言设计

## 1. 概述

AST 查询语言是 PPDB 的一个高级功能，它提供了一个灵活的脚本化查询接口。该语言基于抽象语法树(AST)实现，支持函数式编程范式，并与 PPDB 的分层架构无缝集成。

## 2. 架构设计

### 2.1 层级关系

```
peer 层 (协议+查询语言)
  |
  |- ast_interpreter (查询解释器)
  |- peer_protocol   (网络协议)
  |- peer_server     (服务管理)
  |
storage 层 (存储接口)
  |
engine 层 (执行引擎)
  |
base 层 (基础设施)
```

AST 查询语言主要在 peer 层实现，通过标准接口与 storage 层交互。

### 2.2 核心组件

1. 数据类型系统：
```c
typedef enum {
    VAL_NIL,     // 空值
    VAL_NUM,     // 数字
    VAL_FUN,     // 函数
    VAL_ERR,     // 错误
    VAL_TABLE,   // 表对象
    VAL_ROW,     // 行对象
    VAL_COLUMN,  // 列对象
    VAL_INDEX,   // 索引对象
    VAL_CURSOR,  // 游标对象
    VAL_BYTES,   // 二进制数据
    VAL_STRING   // 字符串
} ValueType;
```

2. 查询处理器：
```c
typedef struct PeerQueryProcessor {
    ASTInterpreter* ast;        // AST解释器
    StorageInterface* storage;  // 存储接口
    ResultConverter* converter; // 结果转换器
    PeerSession* session;      // 会话上下文
} PeerQueryProcessor;
```

3. 存储接口：
```c
typedef struct StorageInterface {
    TableHandle* (*open_table)(const char* name);
    void (*close_table)(TableHandle* table);
    StorageCursor* (*create_cursor)(TableHandle* table);
    bool (*seek_key)(StorageCursor* cursor, const void* key);
    const void* (*get_value)(StorageCursor* cursor);
    StorageTx* (*begin_tx)(void);
    bool (*commit_tx)(StorageTx* tx);
    void (*rollback_tx)(StorageTx* tx);
} StorageInterface;
```

## 3. 功能特性

### 3.1 基本语法

1. 数字字面量：`42`
2. 函数定义：`lambda(x, expr)`
3. 条件分支：`if(cond, then_expr, else_expr)`
4. 局部变量：`local(name, value)`
5. 函数调用：`func(arg1, arg2, ...)`

### 3.2 内置函数

1. 表操作：
   - create_table(name, schema)
   - drop_table(name)
   - alter_table(name, new_schema)

2. 数据操作：
   - insert(table, values)
   - update(table, set_expr, where_expr)
   - delete(table, where_expr)
   - select(table, columns, where_expr)

3. 索引操作：
   - create_index(table, columns)
   - drop_index(name)

4. 事务控制：
   - begin()
   - commit()
   - rollback()

## 4. 实现机制

### 4.1 查询执行流程

1. 解析阶段：
   - 词法分析
   - 语法分析
   - AST构建

2. 执行阶段：
   - 环境准备
   - AST求值
   - 结果转换

3. 结果处理：
   - 数据格式化
   - 错误处理
   - 资源清理

### 4.2 事务管理

```c
typedef struct TransactionContext {
    StorageTx* storage_tx;    // 存储层事务
    ASTEnv* ast_env;         // AST环境
    PeerSession* session;    // 会话信息
    bool is_active;
    IsolationLevel level;
} TransactionContext;
```

### 4.3 错误处理

```c
typedef enum {
    ERR_OK = 0,
    // AST错误 (1-1000)
    ERR_AST_SYNTAX = 1,
    ERR_AST_TYPE = 2,
    // PPDB错误 (1001-2000)
    ERR_PPDB_IO = 1001,
    ERR_PPDB_LOCK = 1002,
    // 通用错误 (2001+)
    ERR_MEMORY = 2001,
    ERR_TIMEOUT = 2002
} ErrorCode;
```

## 5. 性能优化

1. 查询优化：
   - AST重写优化
   - 执行计划优化
   - 缓存机制

2. 内存管理：
   - 内存池
   - 引用计数
   - 资源限制

3. 并发控制：
   - 事务隔离
   - 锁管理
   - 死锁检测

## 6. 安全性

1. 访问控制：
   - 用户认证
   - 权限检查
   - 资源限制

2. 输入验证：
   - 语法检查
   - 类型检查
   - 边界检查

## 7. 可扩展性

1. 插件系统：
   - 自定义函数
   - 自定义类型
   - 自定义操作符

2. 协议扩展：
   - 新命令支持
   - 新数据类型
   - 新错误类型

## 8. 监控和调试

1. 性能指标：
   - 解析时间
   - 执行时间
   - 内存使用

2. 日志记录：
   - 错误日志
   - 审计日志
   - 性能日志

## 9. 示例

### 9.1 创建表
```lisp
local(result, 
    create_table("users",
        schema(
            column("id", "int", "primary key"),
            column("name", "string", "not null"),
            column("age", "int")
        )
    )
)
```

### 9.2 查询数据
```lisp
local(users,
    select("users",
        columns("name", "age"),
        where(gt(field("age"), 18))
    )
)
```

### 9.3 事务操作
```lisp
begin(lambda(tx,
    local(result,
        insert("users",
            values(
                field("name", "Alice"),
                field("age", 20)
            )
        )
    ),
    if(eq(result, "ok"),
        commit(tx),
        rollback(tx)
    )
))
``` 