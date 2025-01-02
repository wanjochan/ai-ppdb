# PPDB 开发指南

> 本文档是 PPDB 的开发指南，主要包含开发环境配置、编码规范和错误处理等内容。它与 `BUILD.md` 互补，共同构成开发者必读文档。相关文档：
> - 构建步骤详见 `overview/BUILD.md`
> - 代码改进计划见 `overview/REFACTOR.md`
> - 系统设计见 `overview/DESIGN.md`

## 1. 开发环境配置

### 1.1 编译环境
- 使用 Cosmopolitan 编译器(cosmocc)
- 支持 Windows/Linux/macOS 跨平台编译
- 生成的二进制文件可在多平台运行

### 1.2 环境搭建步骤
0. 为了防止因为目录路径原因导致的重复的意外下载，请先搜索一下代码仓目录是不是已经有解开后的构建工具了
1. 获取必要文件
```bash

# for windows (https://justine.lol/cosmopolitan/windows-compiling.html)

curl -L -o cosmopolitan.zip https://justine.lol/cosmopolitan/cosmopolitan.zip
# - cosmopolitan.h
# - cosmopolitan.a
# - ape.lds
# - crt.o
# - ape.o

# 下载 cross9.zip (https://justine.lol/cosmopolitan/windows-compiling.html)
curl -L -o cross9.zip https://justine.lol/linux-compiler-on-windows/cross9.zip

# for unix (https://github.com/jart/cosmopolitan)
curl -L -o cosmocc.zip https://cosmo.zip/pub/cosmocc/cosmocc.zip

```

2. 设置文件
- 将 cosmopolitan.h, cosmopolitan.a, ape.lds, crt.o, ape.o 放在 build 目录下

3. 验证安装
```bash
# 运行构建脚本
scripts/build.bat test42
```

## 2. 编码规范

### 2.1 代码风格
- 使用4空格缩进
- 函数和变量使用小写字母加下划线
- 宏和常量使用大写字母加下划线
- 每行不超过80字符

### 2.2 命名规范
- 函数名: ppdb_module_action
- 类型名: ppdb_module_t
- 常量名: PPDB_CONSTANT_NAME
- 局部变量: descriptive_name

### 2.3 注释规范
- 文件头部说明文件用途
- 函数头部说明参数和返回值
- 复杂逻辑处添加实现说明
- 使用中文注释便于理解

## 3. 错误处理规范

### 3.1 错误码定义
```c
typedef enum {
    PPDB_OK = 0,              // 操作成功
    PPDB_ERR_INVALID_ARG,     // 无效参数
    PPDB_ERR_OUT_OF_MEMORY,   // 内存不足
    PPDB_ERR_IO,             // IO错误
    PPDB_ERR_CORRUPTED,      // 数据损坏
    PPDB_ERR_MUTEX_ERROR,    // 互斥锁错误
    PPDB_ERR_TABLE_FULL,     // 表已满
    PPDB_ERR_KEY_NOT_FOUND,  // 键不存在
    PPDB_ERR_WRITE_CONFLICT  // 写入冲突
} ppdb_error_t;
```

### 3.2 错误处理原则
1. 参数验证
- 所有公开接口必须验证参数
- 无效参数返回 PPDB_ERR_INVALID_ARG
- 记录详细的错误信息

2. 资源管理
- 分配失败返回 PPDB_ERR_OUT_OF_MEMORY
- 使用 RAII 模式管理资源
- 确保错误时正确清理

3. 错误日志
- 使用统一的日志接口
- 记录错误上下文信息
- 便于问题定位和调试

### 3.3 示例代码
```c
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table, 
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len) {
    // 1. 参数验证
    if (!table || !key || key_len == 0) {
        ppdb_log_error("Invalid arguments: table=%p, key=%p, key_len=%zu",
                      table, key, key_len);
        return PPDB_ERR_INVALID_ARG;
    }

    // 2. 容量检查
    if (table->current_size + key_len + value_len > table->max_size) {
        ppdb_log_error("Table is full: current=%zu, max=%zu",
                      table->current_size, table->max_size);
        return PPDB_ERR_TABLE_FULL;
    }

    // 3. 加锁保护
    pthread_mutex_lock(&table->mutex);

    // 4. 执行操作
    ppdb_error_t err = do_put(table, key, key_len, value, value_len);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to put: err=%d", err);
    }

    // 5. 解锁
    pthread_mutex_unlock(&table->mutex);
    return err;
}
```

## 4. 测试指南

### 4.1 测试类型
1. 单元测试
- 测试单个功能点
- 验证边界条件
- 检查错误处理

2. 集成测试
- 测试组件交互
- 验证数据流
- 检查并发处理

3. 性能测试
- 测试吞吐量
- 测试延迟
- 测试资源使用

### 4.2 测试规范
1. 测试覆盖
- 核心功能100%覆盖
- 错误处理路径覆盖
- 边界条件覆盖

2. 测试命名
- test_module_function
- test_module_scenario
- test_module_error_case

3. 测试结构
```c
void test_case() {
    // 1. 准备测试数据
    // 2. 执行测试操作
    // 3. 验证测试结果
    // 4. 清理测试资源
}
```

## 5. 常见问题和最佳实践

### 5.1 内存管理
- 使用 malloc/free 配对
- 检查内存泄漏
- 避免内存碎片

### 5.2 并发控制
- 最小化锁的范围
- 避免死锁
- 使用原子操作

### 5.3 性能优化
- 批量处理
- 异步操作
- 内存对齐

### 5.4 调试技巧
- 使用日志跟踪
- 检查系统调用
- 分析core dump 

## 构建系统

### 必要文件
项目构建需要以下关键文件（这些文件已包含在仓库中）：
- `crt.o` - C运行时启动文件
- `ape.o` - APE (Actually Portable Executable) 支持文件
- `cosmopolitan.a` - Cosmopolitan 库文件
- `ape.lds` - 链接器脚本

这些文件已在 .gitignore 中设置为例外，确保它们会被提交到仓库中。

### 构建脚本
项目使用 `scripts/build_ppdb.bat` 进行构建。该脚本：
1. 使用 cross9 工具链进行交叉编译
2. 生成静态库 libppdb.a
3. 生成主程序和测试程序
4. 使用 objcopy 生成最终的可执行文件

### 编译选项
- `-fno-pie -no-pie`: 禁用位置无关代码
- `-mno-red-zone`: 禁用红区优化
- `-fno-omit-frame-pointer`: 保留帧指针
- `-nostdlib -nostdinc`: 不使用标准库和标准头文件
- `-include cosmopolitan.h`: 使用 Cosmopolitan 库

### 链接选项
- `--gc-sections`: 移除未使用的代码段
- `-z,max-page-size=0x1000`: 设置页面大小
- `-fuse-ld=bfd`: 使用 BFD 链接器
- `-T ape.lds`: 使用自定义链接器脚本
