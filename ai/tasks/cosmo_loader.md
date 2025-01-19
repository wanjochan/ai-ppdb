# 当前工作追踪

## 项目：APE 加载器开发

### 问题
- 需要开发 APE (Actually Portable Executable) 格式文件的加载器
- 加载器需要处理 APE 头部和 ELF 段
- 当前 find_elf_header() 还是没找到正确的ELF段，需要仔细阅读 repos/cosmpolitan/ape/*.*

根据规范，我们需要修改find_elf_header函数。主要问题是：
- ELF头部是通过shell脚本中的printf语句以八进制转义码的形式编码的
- 这个printf语句必须出现在APE可执行文件的前8192字节中
- 可能有多个printf语句（用于不同架构）

### 最新发现

1. ELF 头部的位置：
   - APE 格式中，ELF 头部可以在两个地方：
     1. 在 `printf` 语句中以八进制转义码的形式编码
     2. 在 APE 头部指定的偏移量处（`elf_off` 字段）
   - 需要先检查 APE 头部指定的偏移量，如果无效再搜索 `printf` 语句

2. 八进制转义序列规则：
   - 必须使用纯 ASCII 字符或八进制转义码
   - 不能使用简化的转义码如 `\n`
   - 可以使用 1-3 位八进制数字
   - 如果后面的字符不是八进制数字，可以使用 2 位八进制数字

3. 改进的代码：
   - 改进了 `parse_octal` 函数，完全符合规范
   - 修改了 `find_elf_header` 函数的搜索策略
   - 添加了更详细的调试输出

### 下一步计划

1. 测试改进：
   - 构建并测试 `test_target.exe`
   - 验证 ELF 头部的正确识别
   - 检查段加载是否正确

2. 可能的问题：
   - 需要验证多架构支持
   - 需要测试不同的 APE 魔数
   - 需要处理更复杂的 `printf` 语句格式

### 参考资料

1. APE 规范：
   - `repos/cosmopolitan/ape/specification.md`：详细的格式规范
   - `repos/cosmopolitan/ape/ape.h`：基本定义
   - `repos/cosmopolitan/ape/ape.internal.h`：内部实现细节
   - `repos/cosmopolitan/ape/loader.c`：参考加载器实现

2. 关键规范要点：
   - APE 魔数：`MZqFpD=`、`jartsr=`、`APEDBG=`
   - ELF 头部必须在前 8192 字节内
   - 支持多架构（AMD64、ARM64）
   - 必须使用标准的八进制转义序列

### 执行计划

1. 完善 ELF 头部搜索：
   - [x] 改进八进制转义序列解析
   - [x] 优化搜索策略
   - [ ] 添加多架构支持

2. 改进段加载：
   - [ ] 验证段对齐
   - [ ] 检查内存权限
   - [ ] 处理重定位

3. 测试计划：
   - [ ] 构建测试用例
   - [ ] 验证不同魔数
   - [ ] 测试多架构支持

### 其他注意事项

- 不要适配操作系统、不要引用 stdc 的库
- 工作目录：`ppdb/src/cosmo/`
- 主要测试命令：
  ```
  cmd /c "build_test_loader.bat & test_loader.exe & test_loader.exe test_target.exe"
  ```

## APE 加载器开发日志

### 当前进展

1. 实现了基本的 APE 头部解析
   - 支持三种魔数: `MZqFpD=`, `jartsr=`, `APEDBG=`
   - 可以正确读取 size 和 elf_off 字段

2. 实现了 ELF 头部搜索
   - 在 printf 语句中搜索嵌入的 ELF 头
   - 支持解析八进制转义序列
   - 添加了详细的调试输出

3. 实现了 ELF 头部验证
   - 检查魔数、类型、机器架构等
   - 验证程序头表的有效性
   - 添加了详细的错误信息

4. 实现了段加载
   - 计算所需内存大小
   - 分配和保护内存
   - 加载段数据
   - 设置正确的段权限

### 当前问题

1. ELF 头部识别问题
   - 无法正确识别 printf 语句中的 ELF 头
   - 八进制转义序列解析可能存在问题
   - 需要更好地理解 APE 规范中的编码方式

2. 调试输出改进
   - 添加了更多的调试信息
   - 显示 printf 语句的上下文
   - 显示八进制序列的解析过程

### 下一步计划

1. 改进 ELF 头部搜索
   - 仔细研究 APE 规范中的编码方式
   - 修复八进制转义序列解析
   - 添加更多验证步骤

2. 完善错误处理
   - 添加更多错误检查
   - 改进错误信息的可读性
   - 确保资源正确清理

3. 改进调试功能
   - 添加更多调试选项
   - 改进内存转储显示
   - 添加段加载的详细日志

### 参考资料

1. APE 规范文档
   - 位置: `repos/cosmopolitan/ape/specification.md`
   - 重点关注 printf 语句中的 ELF 头编码方式

2. APE 链接器脚本
   - 位置: `repos/cosmopolitan_pub/ape.lds`
   - 包含了 ELF 头的生成方式
