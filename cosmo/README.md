# Cosmo 动态加载器

这是一个用于加载和执行动态链接库（DLL）的工具。它支持两种编译方式：使用 cross9 工具链和使用 MinGW-w64 工具链。

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

## 下一步改进

1. 解决编译警告
2. 扩展运行时库功能
3. 添加更多的 DLL 示例
4. 改进错误处理
5. 添加更多的文档和注释

## 依赖

- MinGW-w64 工具链（x86_64-13.2.0-release-win32-seh-msvcrt-rt_v11-rev0）
- Windows API（kernel32.dll） 