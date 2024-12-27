# TinyCC Cosmopolitan 构建说明

## 准备工作

1. 确保以下目录结构:
   ```
   d:/dev/ai-ppdb/
   ├── ppdb/
   │   ├── cosmopolitan/     # cosmopolitan 库
   │   └── cross9/          # 交叉编译工具链
   ├── cosmo_tinycc/        # TinyCC 源码
   └── lab/                 # 构建脚本和补丁
   ```

2. 应用补丁
   ```bash
   cd d:/dev/ai-ppdb/cosmo_tinycc
   patch -p1 < ../lab/configure.patch
   ```

3. 确认文件
   - cosmo-config.h 已添加
   - tccape.c 已添加
   - Makefile 已更新

## 构建步骤

1. 运行构建脚本:
   ```bash
   cd d:/dev/ai-ppdb/lab
   ./build_cosmo_tcc.bat
   ```

2. 检查输出
   - 确保没有编译错误
   - 检查生成的可执行文件

## 常见问题

1. configure 失败
   - 检查补丁是否正确应用
   - 确认路径设置正确

2. 编译错误
   - 检查 cosmopolitan 库路径
   - 确认交叉编译工具链可用

3. 链接错误
   - 检查 cosmopolitan.a 是否存在
   - 确认链接选项正确

## 下一步

1. 测试编译器
   ```bash
   ./tcc -v
   ./tcc test.c -o test.exe
   ```

2. 验证生成的可执行文件
   - 检查是否为 APE 格式
   - 测试跨平台运行
