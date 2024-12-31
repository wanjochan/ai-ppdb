# Cosmo - 跨平台动态链接库加载器

Cosmo 是一个跨平台的动态链接库加载器，它可以在不同的操作系统上加载和执行 APE (Actually Portable Executable) 格式的动态链接库。

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
   Module size: 172 bytes
   Calling module_main at 0x100080040000
   Module returned: 42
   ```

3. 常见问题排查
   - 如果看不到输出，检查 test.dat 是否在正确位置
   - 如果返回值不对，检查模块是否正确编译
   - 如果出现段错误，检查内存映射权限

## 主要目标

参考 Cosmopolitan 的 APE 机制，实现一个跨平台的动态库解决方案。该方案将允许同一个动态库文件在不同操作系统上运行，无需重新编译或修改。

## 当前进展

### 已完成功能

1. **基础框架**
   - ✅ 基本的模块加载器 (cosmo.exe)
   - ✅ 简单的模块生成系统
   - ✅ 基本的内存映射和执行
   - ✅ Windows 平台验证

2. **构建系统**
   - ✅ 加载器构建脚本 (build_cosmo.bat)
   - ✅ 模块构建脚本 (build.bat)
   - ✅ 链接器脚本 (dll.lds)

3. **基本功能验证**
   - ✅ 代码段加载和执行
   - ✅ 基本的函数调用
   - ✅ 返回值处理

### 进行中的工作

1. **模块格式完善**
   - ⏳ APE 头部生成
   - ⏳ 段表优化
   - ⏳ 导出表实现

2. **加载器增强**
   - ⏳ 符号解析
   - ⏳ 重定位支持
   - ⏳ 错误处理

## 下一步计划

### 1. 完善模块格式
- 实现完整的导出表
- 支持多函数导出
- 添加版本信息支持
- 实现调试信息

### 2. 增强加载器功能
- 改进内存管理
- 添加安全检查
- 实现错误恢复
- 支持动态符号解析

### 3. 跨平台支持
- 添加 Linux 支持
- 实现平台检测
- 处理系统调用差异

## 构建说明

### 构建加载器
```powershell
.\build_cosmo.bat
```

### 构建测试模块
```powershell
.\build.bat test
```

### 运行测试
```powershell
.\cosmo.exe test.dat
```

## 模块格式说明

### 当前格式
```
[代码段]
- module_main 函数
- 其他导出函数
[数据段]
- 常量数据
- 变量数据
```

### 计划格式
```
[APE 头部]
- DOS 头
- PE 头
- ELF 头
[代码段]
- 导出函数
[数据段]
- 常量
- 变量
[导出表]
- 函数符号
- 版本信息
```

## 开发规范

### 代码组织
- 加载器代码：cosmo.c
- 测试模块：test.c
- 构建脚本：build*.bat
- 链接脚本：dll.lds

### 命名规范
- 函数名：小写字母，下划线分隔
- 常量：大写字母，下划线分隔
- 变量：小写字母，下划线分隔

### 文档规范
- 重要函数必须有注释
- 更新必须反映在 README.md
- 重要变更必须记录

## 调试技巧

### 常见问题
1. 加载失败
   - 检查文件路径
   - 验证文件格式
   - 检查内存权限

2. 执行错误
   - 检查段对齐
   - 验证函数入口
   - 检查内存映射

### 调试命令
```powershell
# 查看模块内容
objdump -x build/test.dbg | cat

# 检查二进制文件
hexdump -C test.dat | head -n 20
``` 