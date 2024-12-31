# Cosmo 动态加载器

这是一个用于加载和执行动态链接库（DLL）的工具。它支持两种编译方式：使用 cross9 工具链和使用 MinGW-w64 工具链。

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