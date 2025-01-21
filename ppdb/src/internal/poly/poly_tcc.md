# poly_tcc 组件设计文档

## 1. 概述

poly_tcc 是一个从 TinyCC(TCC) 移植和简化的 C 语言编译器组件。它保持了 TCC 的核心功能,但使用了自己的实现方式。

## 2. 核心功能

从 TCC 移植的核心功能包括:

- 状态管理 (State Management)
- 内存管理 (Memory Management) 
- 符号表管理 (Symbol Table Management)
- 词法分析 (Lexical Analysis)
- 语法分析 (Syntax Analysis) - TODO
- 代码生成 (Code Generation) - TODO

## 3. 实现差异

与 TCC 的主要实现差异:

1. 状态管理
   - 使用自定义的 `poly_tcc_state_t` 结构
   - 简化了状态字段,只保留必要的编译和执行信息
   
2. 内存管理
   - 使用 `poly_tcc_malloc/free` 封装 
   - 使用 `poly_tcc_mmap/munmap/mprotect` 管理代码和数据段

3. 符号表管理  
   - 使用简化版的符号表结构
   - 实现了基本的符号添加、查找和删除功能

4. 词法分析
   - 从字符串读取而不是标准输入
   - 保留了基本的 token 类型定义
   - 实现了标识符、数字、字符串等的解析

## 4. 代码结构

主要源文件:
- `poly_tcc.h`: 头文件,定义接口和数据结构
- `poly_tcc.c`: 实现文件,包含所有功能实现

主要结构体:
```c
typedef struct poly_tcc_state_t {
    // 编译状态
    const char *source;     // 源代码字符串
    int source_len;        // 源代码长度
    int source_pos;        // 当前解析位置
    
    // 词法分析状态
    int tok;              // 当前 token
    CValue tokc;          // token 值
    
    // 内存管理
    void *code;           // 代码段
    int code_size;        // 代码大小
    int code_capacity;    // 代码段容量
    void *data;           // 数据段
    int data_size;        // 数据大小
    int data_capacity;    // 数据段容量
    
    // 符号表
    Sym *global_stack;    // 全局符号栈
    Sym *local_stack;     // 局部符号栈
    Sym *define_stack;    // 宏定义栈
    Sym *global_label_stack;  // 全局标签栈
    Sym *local_label_stack;   // 局部标签栈
    
    // 错误处理
    char error_msg[256];  // 错误信息
} poly_tcc_state_t;
```

## 5. 使用方式

基本使用流程:

1. 创建编译状态:
```c
poly_tcc_state_t *s = poly_tcc_new();
```

2. 编译源代码:
```c
poly_tcc_compile_string(s, source_code);
```

3. 执行编译后的代码:
```c
poly_tcc_run(s, argc, argv);
```

4. 清理:
```c
poly_tcc_delete(s);
```

## 6. TODO 列表

1. 实现完整的词法分析器
   - 完善 `next_token()` 函数
   - 实现从源字符串读取字符

2. 实现语法分析器
   - 添加语法树节点定义
   - 实现基本语法规则解析

3. 实现代码生成器
   - 支持基本的 x86_64 指令生成
   - 实现函数调用约定

4. 添加错误处理和诊断信息
   - 完善错误消息
   - 添加行号和列号跟踪

## 7. 注意事项

1. 不要直接调用 TCC 的函数,使用 poly_tcc 的封装函数
2. 保持简单实现,避免过度优化
3. 专注于基本功能的正确性 