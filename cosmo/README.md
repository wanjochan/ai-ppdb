# Cosmo - 跨平台动态链接库加载器

Cosmo 是一个跨平台的动态链接库加载器，它可以在不同的操作系统上加载和执行 APE (Actually Portable Executable) 格式的动态链接库。

## 主要目标

参考 Cosmopolitan 的 APE 机制，实现一个跨平台的动态库解决方案。该方案将允许同一个动态库文件在不同操作系统上运行，无需重新编译或修改。

## 分步实现计划

1. **第一阶段：基础框架搭建** [当前]
   - 实现基本的 PE 格式 DLL 生成器
   - 实现基本的动态库加载器
   - 验证在 Windows 上的加载和执行

2. **第二阶段：多格式支持**
   - 添加 ELF 格式支持
   - 实现多格式头部的生成和组合
   - 验证在 Linux 上的加载和执行

3. **第三阶段：跨平台运行时**
   - 实现平台检测机制
   - 添加系统调用适配层
   - 处理不同平台的 ABI 差异

4. **第四阶段：工具链集成**
   - 整合 MinGW-w64 和 Cross9 工具链
   - 实现自动化构建流程
   - 提供完整的开发工具套件

## 当前重点工作

正在进行第一阶段的工作，重点是实现和完善 PE 格式 DLL 的生成和加载：

1. **PE DLL 生成器的实现**
   - 完善 APE 头部生成
   - 确保段表和导出表的正确性
   - 实现可靠的文件生成机制

2. **基础加载器的开发**
   - 实现基本的 DLL 加载功能
   - 添加符号解析和重定位支持
   - 提供错误处理和诊断功能

## 当前具体任务

1. 修复 APE DLL 生成器中的问题：
   - 确保 PE 头部和段表的正确对齐
   - 修复 `.text` 和 `.edata` 段的大小和偏移计算
   - 确保导出表的正确生成和放置
   - 验证生成的 DLL 文件格式是否符合 PE 规范

2. 改进 Cosmo 加载器：
   - 增强错误处理和诊断信息
   - 添加更详细的加载过程日志
   - 验证加载的 DLL 是否符合 APE 格式要求

3. 测试和验证：
   - 编写更多的测试用例
   - 验证在不同操作系统上的兼容性
   - 测试不同类型的导出函数

4. 文档完善：
   - 添加详细的设计文档
   - 编写 API 使用指南
   - 记录已知问题和解决方案

## 构建说明

```powershell
# Add MinGW to PATH
$env:PATH += ";D:\dev\ai-ppdb\mingw64\bin"

# Compile
gcc.exe -o cosmo.exe cosmo.c
```

## Tools for Examining APE Files

The following tools can be used to examine APE (Actually Portable Executable) files:

```powershell
# Check if a file exists and view its properties
ls test.dll

# View file contents in hexadecimal format (first 512 bytes)
$bytes = [System.IO.File]::ReadAllBytes("$pwd\test.dll")
for ($i = 0; $i -lt [Math]::Min(512, $bytes.Length); $i += 16) {
    $hex = [BitConverter]::ToString($bytes[$i..([Math]::Min($i + 15, $bytes.Length - 1))])
    Write-Host ("{0:X8}: {1}" -f $i, $hex.Replace("-", " "))
}

# Examine object file format (MinGW)
objdump.exe -x test.dll | cat

# Display printable strings (MinGW)
strings.exe test.dll | cat
```

## APE File Format

The APE file format combines DOS, ELF, and PE headers to create a multi-format executable that can be loaded on different platforms. The file layout is as follows:

```
Offset    Size    Description
0x0000    0x40    DOS Header + DOS Stub
0x0040    0x40    ELF Header
0x0080    0x20    PE Header
0x00A0    0xF0    PE Optional Header
0x0180    0x28    PE Section Headers (.text)
0x01A8    0x28    PE Section Headers (.edata)
0x2000    varies  Export Directory
0x4000    varies  Code Section
```

## Debugging APE Files

When examining generated APE files, check the following:

1. DOS Header magic number (should be "MZ")
2. PE Header signature (should be "PE\0\0")
3. Section alignments and sizes
4. Export directory RVAs and contents
5. Code section placement and alignment

## APE (Actually Portable Executable) 机制

Cosmopolitan 使用的 APE 机制是一个优雅的跨平台解决方案。它的工作原理是：

1. **多格式头部**：
   - 文件同时包含 DOS/PE、ELF、MachO 等头部
   - 每个平台都能识别并加载对应的头部
   - 不同平台看到的是同一个文件的不同"视角"

2. **统一代码段**：
   - 只包含一份 x86_64 机器码
   - 代码本身是平台无关的
   - 通过运行时适配层处理平台差异

3. **系统调用适配**：
   - 运行时检测当前平台
   - 动态选择合适的系统调用实现
   - 统一的 API 接口，不同的底层实现

## 使用 MinGW-w64 编译 DLL

我们成功使用 MinGW-w64 工具链编译了可以被 Cosmo 加载的 DLL。以下是关键步骤和经验：

### 1. 最小运行时库

为了避免依赖标准库，我们创建了一个最小的运行时库，包含：

- `runtime.h`：声明基本类型和函数
- `runtime.c`：实现必要的函数
  - 字符串操作：`strlen`, `memcpy`
  - 输出函数：`puts`, `printf`（支持 %s 和 %d）
  - 使用 Windows API 函数进行实际的控制台输出

### 2. DLL 导出

DLL 需要导出 `module_main` 函数作为入口点：

```c
__declspec(dllexport)
int module_main(int argc, char* argv[]) {
    printf("Hello from DLL!\n");
    return 0;
}
```

### 3. 编译脚本

使用 `build_dll2.bat` 进行编译，关键步骤：

1. 编译运行时库：
```batch
gcc -c -o runtime.o runtime.c
```

2. 编译 DLL：
```batch
gcc -shared -o main2.dll main2.c runtime.o -lkernel32
```

### 4. 注意事项

1. 运行时库实现需要小心处理：
   - 可变参数的安全处理
   - 字符串缓冲区的边界检查
   - NULL 指针检查

2. 虽然有一些编译警告（如 strlen 和 memcpy 的类型冲突），但不影响 DLL 的正常运行

3. DLL 入口点的命名很重要：
   - Cosmo 会按以下顺序查找入口点：`module_main`、`_module_main`
   - 确保使用正确的导出属性 `__declspec(dllexport)`

## 未来发展计划

### 1. APE 动态库支持

计划实现类似 Cosmopolitan 的 APE 机制用于动态库：

1. **多格式动态库头部**：
   - DOS/PE 格式 (Windows DLL)：
     * MZ Header + DOS Stub
     * PE Header + Optional Header
     * Section Headers (.text, .data 等)
   - ELF 格式 (Linux .so)：
     * ELF Header
     * Program Headers
     * Section Headers
   - Mach-O 格式 (macOS .dylib)：
     * Mach Header
     * Load Commands
     * Segments

2. **头部布局设计**：
   ```
   [0x0000] DOS MZ Header (Windows)
   [0x0040] ELF Header (Linux)
   [0x0080] PE Header (Windows)
   [0x00C0] Mach-O Header (macOS)
   [.......] 共享代码段
   ```

3. **运行时适配层**：
   - 平台检测机制
   - 统一的系统调用接口
   - ABI 差异处理

### 2. 实现路线图

1. **第一阶段**：头部格式研究
   - 分析 Cosmopolitan 的头部实现
   - 提取关键的头部生成代码
   - 创建简单的测试用例

2. **第二阶段**：基础实现
   - 实现基本的多格式头部生成
   - 创建头部模板和工具
   - 验证各平台的加载

3. **第三阶段**：工具链集成
   - 整合 cross9 (ELF) 和 MinGW-w64 (PE)
   - 实现头部合并工具
   - 自动化构建流程

4. **第四阶段**：运行时支持
   - 实现平台检测
   - 添加系统调用适配
   - 处理 ABI 差异

### 3. 当前任务

1. **研究 Cosmopolitan**：
   - 分析 `ape.h` 和相关头部定义
   - 理解头部生成机制
   - 提取可复用的代码

2. **实验性实现**：
   - 创建最小的头部模板
   - 实现基本的合并工具
   - 验证加载可行性

## 依赖

- MinGW-w64 工具链（x86_64-13.2.0-release-win32-seh-msvcrt-rt_v11-rev0）
- Cross9 工具链
- Windows API（kernel32.dll）
- Cosmopolitan 库（参考实现） 