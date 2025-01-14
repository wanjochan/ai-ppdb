# Cosmopolitan库使用说明

## 跨平台原理

Cosmopolitan通过巧妙的设计实现了真正的"build once, run anywhere"：

1. 统一ABI
- 使用x86-64 Linux ABI作为统一的"通用语言"
- 所有平台通过SYSCALL指令进行系统调用
- 避免了传统跨平台方案中的API适配问题

2. 多格式文件头
- 文件开头的`MZqFpD`既是DOS头也是合法的x86指令
- ELF头部被设计成合法的x86指令
- 通过这种方式实现了在不同系统上的自动识别

3. 平台支持方式
- x86-64平台：直接运行本地代码
- ARM64平台（如Apple Silicon、树莓派）：
  - 通过内嵌的x86模拟器运行
  - 自动检测并切换到模拟模式
  - 无需用户干预

4. APE (Actually Portable Executable) 格式
- 单一二进制文件支持多个平台
- 不需要虚拟机或解释器
- 保持了接近原生的性能

## 使用方法

1. 编译选项
```batch
-static -nostdlib -Wl,-T,%BUILD_DIR%\ape.lds -Wl,--gc-sections -Wl,--build-id=none
```

2. 链接要求
- 链接`ape.o`提供多格式支持
- 链接`cosmopolitan.a`提供运行时支持

3. 注意事项
- 不需要为不同架构编译不同版本
- 不需要特殊的交叉编译工具链
- ARM平台支持是自动的，通过内嵌模拟器实现

## 优势
1. 真正的跨平台：支持Linux、Windows、MacOS、BSD等
2. 简单部署：单一二进制文件
3. 高性能：x86-64平台原生执行
4. 优雅降级：ARM平台自动切换到模拟模式 