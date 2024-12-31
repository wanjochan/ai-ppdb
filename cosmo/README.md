# Cosmo - ELF Loader for Windows

跨平台加载和执行 ELF 文件的动态加载器
（暂时在做x64版本）
（如果成功后还会开发跨平台的类似ape原理的动态库，兼容dll/dyld/so）

智能助理注意：还要阅读docs/*.md进行知识热身

## 当前进展

### 已完成功能
- ELF 文件格式解析
- 程序头(Program Headers)处理
- 内存映射和段加载
- 基本的符号解析
- 代码段(.text)执行
- 数据段(.data)读写
- BSS 段初始化
- 全局变量支持
- 基本函数调用支持

### 调试工具
```bash
# 查看 ELF 文件头信息
..\cross9\bin\x86_64-pc-linux-gnu-readelf.exe -h test2.elf

# 查看程序头信息
..\cross9\bin\x86_64-pc-linux-gnu-readelf.exe -l test2.elf

# 查看节头信息
..\cross9\bin\x86_64-pc-linux-gnu-readelf.exe -S test2.elf

# 查看符号表
..\cross9\bin\x86_64-pc-linux-gnu-readelf.exe -s test2.elf

# 查看代码段反汇编
..\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -d test2.elf
```

### 测试用例
1. test.c - 基本功能测试
   - 简单的返回值测试
   - 基本函数调用测试

2. test2.c - 高级功能测试
   - 全局变量测试
   - 基本数学运算测试
   - 全局状态测试
   - 指针操作测试

## 待实现功能

### 近期计划
1. 内存管理优化
   - 确保使用正确的基址(0x220000000)
   - 优化页面对齐处理
   - 完善内存保护属性设置

2. 段加载增强
   - 完善所有必要段的加载
   - 优化段的对齐要求
   - 完善段的权限处理

3. 符号处理增强
   - 实现重定位表处理
   - 实现动态符号表处理
   - 支持导入/导出函数

4. 异常处理支持
   - 加载异常处理相关段
   - 设置异常处理框架

### 长期计划
1. 动态链接支持
   - 实现动态库加载
   - 处理动态链接重定位
   - 支持延迟绑定

2. 调试支持
   - 加载调试信息
   - 支持调试器附加
   - 提供调试API

3. 性能优化
   - 优化内存使用
   - 优化加载时间
   - 实现缓存机制

4. 安全增强
   - 实现ASLR支持
   - 加强内存保护
   - 实现安全检查

## 构建说明

### 环境要求
- Windows 10/11
- GCC Cross Compiler (x86_64-pc-linux-gnu)
- PowerShell 或 CMD

### 构建步骤
1. 构建加载器
```bash
gcc -o cosmo.exe cosmo.c
```

2. 构建测试模块
```bash
.\build.bat test2
```

3. 运行测试
```bash
.\cosmo.exe test2.elf
```

## 技术细节

### 内存布局
- 基址：0x220000000
- 代码段：R-X
- 数据段：RW-
- BSS段：RW-

### 重要函数
- `map_memory`: 内存映射
- `verify_elf_header`: ELF头验证
- `load_segment`: 段加载
- `resolve_symbols`: 符号解析

### 文件说明
- `cosmo.c`: 加载器主程序
- `dll.lds`: 链接器脚本
- `build.bat`: 构建脚本
- `test.c`: 基本测试
- `test2.c`: 高级测试 