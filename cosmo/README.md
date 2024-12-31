# Cosmo - 跨平台动态链接库加载器

Cosmo 是一个跨平台的动态链接库加载器，它可以在不同的操作系统上加载和执行 ELF 格式的动态链接库。

## 快速开始

### 环境要求
- Windows 10/11
- Cross9 工具链（用于编译）
- PowerShell 7+

### 目录结构
```
cosmo/
├── build/          # 构建输出目录
├── cosmo.c         # 加载器主程序
├── test.c          # 测试模块
├── dll.lds         # 链接器脚本
├── build.bat       # 模块构建脚本
└── build_cosmo.bat # 加载器构建脚本
```

### 重要文件说明
- `cosmo.c`: 动态库加载器的核心实现，负责加载和执行模块
- `test.c`: 测试模块示例，展示了基本的模块结构和导出函数
- `dll.lds`: 链接器脚本，定义了模块的内存布局和段结构
- `build_cosmo.bat`: 用于构建加载器的脚本
- `build.bat`: 用于构建测试模块的脚本

### 快速测试
1. 构建加载器
   ```powershell
   .\build_cosmo.bat
   ```

2. 构建测试模块
   ```powershell
   .\build.bat test
   ```

3. 运行测试
   ```powershell
   .\cosmo.exe test.dat
   ```

### 测试用例说明
1. 基本功能测试 (test.c)
   ```c
   // 主入口函数
   int module_main(void) {
       return 42;  // 应该看到返回值 42
   }

   // 导出函数示例
   int test_func1(int x) {
       return x * 2;
   }

   int test_func2(int x, int y) {
       return x + y;
   }
   ```

2. 预期输出
   ```
   Loading module: test.dat
   Module size: 4584 bytes
   ELF header verified
   Mapped module at: 0x110000000
   Section 0:  at offset 0x0, addr 0x0
   Section 1: .text at offset 0x1000, addr 0x110000000
   Loaded section to 0x220000000 (size: 11)
   Found module_main at virtual address 0x220000000
   Calling module_main at 0x220000000
   Module returned: 42
   ```

## 主要目标

实现一个跨平台的动态库解决方案，允许同一个动态库文件在不同操作系统上运行。目前专注于基本的 ELF 加载和执行功能。

## 当前进展

### 已完成功能

1. **基础框架**
   - ✅ 基本的 ELF 加载器 (cosmo.exe)
   - ✅ 简单的模块生成系统
   - ✅ 基本的内存映射和执行
   - ✅ Windows 平台验证

2. **构建系统**
   - ✅ 加载器构建脚本 (build_cosmo.bat)
   - ✅ 模块构建脚本 (build.bat)
   - ✅ 链接器脚本 (dll.lds)

3. **基本功能验证**
   - ✅ ELF 文件解析
   - ✅ 代码段加载和执行
   - ✅ 基本的函数调用
   - ✅ 返回值处理

### 进行中的工作

1. **功能增强**
   - ⏳ 输出功能（Windows API）
   - ⏳ 函数导出表
   - ⏳ 重定位支持
   - ⏳ 错误处理改进

## 下一步计划

### 1. 输出功能
- 实现基本的控制台输出
- 使用 Windows API 进行输出
- 添加格式化输出支持

### 2. 函数导出
- 实现导出表结构
- 支持多函数导出
- 添加符号查找功能

### 3. 重定位支持
- 实现基本重定位
- 处理相对地址
- 支持动态基址

### 4. 错误处理
- 改进错误检测
- 添加错误恢复
- 完善错误信息

## 技术细节

### 内存布局
- 代码段：0x220000000
- 数据段：紧随代码段
- 页面大小：4KB 对齐

### 重要函数
- `module_main`: 模块入口点
- `load_section`: 加载 ELF 段
- `main`: 加载器主函数

### 构建参数
- 编译选项：-fPIC -nostdlib -nostdinc
- 链接选项：-static -T dll.lds --gc-sections

## 调试技巧

### 常见问题
1. 加载失败
   - 检查 ELF 头
   - 验证段对齐
   - 检查内存权限

2. 执行错误
   - 检查入口点地址
   - 验证段加载
   - 检查返回值

### 调试命令
```powershell
# 查看 ELF 文件头和段信息
..\cross9\bin\x86_64-pc-linux-gnu-readelf.exe -a build\test.o

# 查看指定段的反汇编代码
..\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -d -j .text build\test.o

# 查看 ELF 文件的段信息
..\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -h build\test.elf

# 检查段内容
..\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -s -j .text build\test.elf

# 查看符号表
..\cross9\bin\x86_64-pc-linux-gnu-nm.exe build\test.o

# 查看重定位信息
..\cross9\bin\x86_64-pc-linux-gnu-readelf.exe -r build\test.o
```

### 调试输出说明
1. readelf 输出
   - ELF Header: 文件头信息，包括类型、机器架构等
   - Section Headers: 段表信息，包括段的偏移、大小、权限等
   - Symbol Table: 符号表，包括函数和变量的名称和地址
   - Relocation section: 重定位信息，用于修正代码中的地址引用

2. objdump 输出
   - 段信息: 大小、VMA、LMA、对齐等
   - 反汇编: 指令序列，用于验证代码正确性
   - 原始内容: 十六进制显示，用于检查数据

3. nm 输出
   - T: 代码段符号
   - D: 数据段符号
   - U: 未定义符号

### 常见错误和解决方法
1. SIGILL (Illegal Instruction)
   - 检查代码段加载地址是否正确
   - 验证内存权限设置
   - 确保代码对齐正确

2. 段加载错误
   - 检查链接器脚本中的段布局
   - 验证段的对齐要求
   - 确保段的加载地址不冲突

3. 符号解析错误
   - 检查符号表是否完整
   - 验证符号名称是否正确
   - 确保链接器脚本包含所有必要的段 