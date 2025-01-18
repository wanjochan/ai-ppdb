# Cosmo动态加载器

## 项目目标

利用Cosmopolitan工具链，aper_loader()，
实现加载其他用cosmopolitan编译的ape格式的可执行文件并调用。
该计划是是PPDB（跨平台动态库模块）的一个组成部分。
参考 https://github.com/jart/cosmopolitan/blob/master/ape/loader.c
请注意你的开发环境是在 windows，正在 cursor 编辑器中的composer所以用的是 powershell

### 核心目标
brev.

## 插件格式


## 开发规范

### 插件开发规范
1. 命名规范
   - 插件文件后缀统一使用`.dl`
   - 导出函数必须使用`dl_`前缀（如`dl_init`、`dl_main`、`dl_fini`）
   - 内部函数使用`static`修饰，避免符号冲突
   - 变量和函数使用小写字母和下划线命名

2. 函数导出规范
   - 必须导出的函数：
     * `dl_init`：初始化函数，返回0表示成功
     * `dl_main`：主函数，返回值由插件定义
     * `dl_fini`：清理函数，返回0表示成功

3. 内存使用规范
   - 避免使用全局变量，优先使用函数参数传递数据

4. 错误处理规范
   - 使用返回值传递错误状态，0表示成功
   - 错误信息使用标准输出（stdout/stderr）
   - 发生错误时必须清理已分配资源
   - 提供详细的错误描述

### 构建规范
1. 编译选项
   ```bat
   必选项：
   -fno-pic -fno-pie        # 禁用位置无关代码
   -nostdinc               # 不使用标准库头文件
   -ffunction-sections     # 每个函数单独段
   -fdata-sections        # 每个数据单独段
   
   建议项：
   -O0                    # 调试时禁用优化
   -fno-inline           # 禁用内联
   -Wall -Wextra         # 启用警告
   ```

2. 链接选项
   ```bat
   必选项：
   -static -nostdlib      # 静态链接，不使用标准库
   -T dl.lds             # 使用自定义链接脚本
   --gc-sections         # 删除未使用的段
   
   建议项：
   --build-id=none      # 不生成build-id
   -z,max-page-size=4096 # 页面大小设置
   ```


## 开发经验总结（但可能有错！）


### 运行时支持
1. 包装函数
   - 需要实现`__wrap_main`和`__wrap__init`
   - 需要实现`__wrap_ape_stack_round`
   - 需要实现`__wrap___cxa_atexit`（如果使用C++特性）

2. 标准库函数
   - 简单函数（如write）可直接使用
   - 复杂函数（如printf）需要确保运行时支持
   - 使用静态链接确保包含所有依赖

### 调试技巧
1. 段分析
   - 使用objdump检查段的存在
   - 验证段的大小和对齐
   - 检查段的权限设置

2. 符号分析
   - 检查函数是否被正确导出
   - 验证偏移量计算
   - 确认符号未被优化掉

### 最佳实践
1. 插件设计
   - 保持插件接口简单明确
   - 使用生命周期函数（init/main/fini）
   - 避免复杂的全局状态

2. 构建系统
   - 使用统一的工具链版本
   - 保持编译选项一致
   - 添加基本的错误检查

3. 测试验证
   - 从简单功能开始测试
   - 逐步添加复杂特性
   - 保留测试用例作为参考 


### 未来规划
1. P2P网络实现
   - 使用IPFS协议实现节点发现
   - 构建私有化网络
   - 实现数据包分发

2. 升级系统
   - 基于P2P网络的升级包分发
   - 增量更新支持
   - 版本控制和回滚机制

3. 插件生态
   - 标准化插件开发流程
   - 插件市场和分发机制
   - 插件安全审计系统 

## APE Loader 实现记录

### 相关文件
```
ppdb/src/cosmo/
├── ape_loader.c     # APE加载器实现
├── ape_loader.h     # 加载器头文件
├── test_loader.c    # 测试主程序
├── test_target.c    # 被加载的目标程序
└── build_test_loader.bat  # 构建脚本
```

### 构建命令
```bat
@echo off
set GCC=..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe
set LD=..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-ld.exe
set OBJCOPY=..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe

%GCC% -g -Os -fno-pie -no-pie -static test_loader.c ape_loader.c -o test_loader.exe
```

### 当前问题
1. ELF头部定位问题:
   - 需要在PE头部之后正确定位ELF头部
   - 当前使用多个固定偏移量(0x1000-0xf000)搜索
   - 需要理解APE文件格式的具体布局

2. 函数地址解析:
   - 目前使用固定偏移量(0x7afe)
   - 需要实现通过符号表查找函数
   - 需要正确处理重定位

### 下一步计划
1. 分析APE文件格式,确定ELF头部的准确位置
2. 实现符号表解析
3. 完善内存映射和权限设置
4. 添加错误处理和调试信息

### 参考资料
1. APE Loader实现: https://github.com/jart/cosmopolitan/blob/master/ape/loader.c
2. APE文件格式: https://justine.lol/ape.html 