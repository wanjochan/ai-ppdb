# 当前工作追踪

## 项目：APE 加载器开发

### 问题
- 需要开发 APE (Actually Portable Executable) 格式文件的加载器
- 加载器需要处理 APE 头部和 ELF 段
- 当前 find_elf_header() 还是没找到正确的ELF段，需要仔细阅读 repos/cosmpolitan/ape/*.*
```
根据规范，我们需要修改find_elf_header函数。主要问题是：
ELF头部是通过shell脚本中的printf语句以八进制转义码的形式编码的
这个printf语句必须出现在APE可执行文件的前8192字节中
可能有多个printf语句（用于不同架构）


```

### 分析
1. APE 格式结构：
   - APE 头部魔数为 "MZqFpD="
   - ELF 文件嵌入在特定偏移位置
   - 需要正确提取和加载 ELF 段

2. 实现要求：
   - Windows 兼容的内存分配
   - 正确的段加载和权限设置
   - 错误处理和验证

### 解决方案
修正测试加载器 (test_loader.c)：
   - 文件读取和内存分配
   - APE 头部验证
   - ELF 段加载和执行

### 执行
1. 当前状态：
   - 基本的 APE 头部解析已实现
   - ELF 段加载部分工作
   - 正在解决内存分配问题

2. 自动化执行计划：
   a. 内存分配改进
      - 实现页面对齐
      - 添加内存权限设置
      - 完善错误处理
   
   b. 段加载完善
      - 完成 load_elf_segments 函数
      - 添加段验证
      - 实现段对齐
   
   c. 错误处理增强
      - 添加详细错误信息
      - 实现资源清理
      - 添加健壮性检查

3. 执行顺序：
   - 先完成内存分配改进
   - 然后实现段加载
   - 最后增强错误处理
   
4. 测试方案：
```
- 不要怀疑没有工具链，只是你工作目录不对！！
- 验证内存映射是否正确
- 检查段加载是否成功
- 确认错误处理是否有效
工作目录 ppdb/src/cosmo/
主要测试命令
cmd /c "build_test_loader.bat & test_loader.exe & test_loader.exe test_target.exe"
或者分解（下面例子是在powershell中执行）
.\build_test_loader.bat
.\test_loader.exe test_target.exe
```

### 其他注意

```
不要适配操作系统、不要引用 stdc 的库
相关文档：
ppdb\src\cosmo\cosmo.md
参考资料：
repos/cosmpolitan/ape/*.*
```
