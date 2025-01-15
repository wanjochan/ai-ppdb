# Cosmo动态加载器

## 项目目标

利用Cosmopolitan工具链，实现一个轻量级的动态加载器，用于加载和执行特定格式的插件文件。该项目是PPDB（跨平台动态库模块）的一个组成部分。

### 核心目标
1. 实现简单的插件格式和加载机制
2. 插件文件包含完整的静态编译代码（包含cosmopolitan库）
3. 支持标准的生命周期管理（init/main/fini）
4. 确保与Cosmopolitan工具链的兼容性

### 设计思路
1. 使用简单的二进制格式作为插件格式
   - 固定的头部结构（magic、version、offsets）
   - 紧凑的代码段布局
   - 静态链接所有依赖
2. 实现基本的插件加载和执行
   - 文件映射和权限管理
   - 函数调用约定
   - 错误处理机制
3. 提供生命周期管理接口
   - dl_init：初始化
   - dl_main：主函数
   - dl_fini：清理

## 插件格式

### 头部结构（20字节）
```c
struct plugin_header {
    uint32_t magic;    // 0x50504442 ("PPDB")
    uint32_t version;  // 当前为1
    uint32_t init_offset;   // dl_init函数偏移
    uint32_t main_offset;   // dl_main函数偏移
    uint32_t fini_offset;   // dl_fini函数偏移
};
```

### 文件布局
1. 头部段（.header）
   - 包含plugin_header结构
   - 位于文件开始处
2. 代码段（.text）
   - 包含插件函数
   - 包含静态链接的库代码
3. 数据段（.data）
   - 包含只读数据
4. BSS段（.bss）
   - 包含未初始化数据

## 构建系统

### 编译选项
- -fno-pic -fno-pie：生成位置无关代码
- -nostdinc：不使用标准库头文件
- -ffunction-sections：每个函数放入单独的段
- -static -nostdlib：静态链接，不使用标准库

### 链接脚本
1. 段的布局
   - .header段必须在最前面
   - 其他段按需排列
2. 偏移量计算
   - 使用ADDR和SIZEOF计算函数偏移
   - 确保正确的段对齐

### 工具链
- 使用Cosmopolitan的交叉编译工具
- gcc用于编译
- ld用于链接
- objcopy用于生成最终二进制

### 调试工具
1. dump_header.bat：分析插件头部
   - 显示文件基本信息
   - 查看头部十六进制内容
   - 分析ELF头部结构

2. dump_sections.bat：分析段布局
   - 显示所有段的信息
   - 检查段的大小和对齐
   - 验证段的权限

3. dump_symbols.bat：分析符号表
   - 显示导出符号
   - 检查函数地址
   - 验证符号可见性

### 验证流程
1. 编译验证
   - 使用build_test11.bat编译
   - 检查生成文件的大小
   - 验证编译警告和错误

2. 格式验证
   - 使用dump_header.bat检查头部
   - 使用dump_sections.bat检查段布局
   - 使用dump_symbols.bat检查符号

3. 加载验证
   - 检查mmap是否成功
   - 验证内存权限
   - 确认函数地址

4. 执行验证
   - 检查init返回值
   - 验证main功能
   - 确认fini清理

## 注意事项

1. 编译和链接
   - 确保正确链接cosmopolitan.a
   - 保持正确的段对齐
   - 验证生成的二进制格式

2. 调试建议
   - 使用objdump检查段布局
   - 使用hexdump验证文件格式
   - 添加详细的运行时日志

## 已知限制
1. 插件文件较大（包含完整的静态库）
2. 仅支持基本的函数调用
3. 没有符号导入/导出机制
4. 错误处理机制需要改进

## 下一步计划
1. 完善插件加载机制
2. 改进错误处理
3. 添加更多的调试功能
4. 考虑添加符号导出机制

## 链接脚本设计经验

从ape.lds中学到的重要经验：

1. 段的布局和对齐
   - 使用ADDR()和LOADADDR()计算段的地址
   - 使用ALIGN()确保段的对齐
   - 使用SIZEOF()计算段的大小

2. 符号的定义和使用
   - 使用PROVIDE()定义全局符号
   - 使用PROVIDE_HIDDEN()定义局部符号
   - 使用BYTE()直接写入数据

3. 重定位和偏移量
   - 使用 `. - ADDR(section)` 计算相对偏移
   - 使用符号标记关键位置
   - 使用DISCARD处理不需要的段

4. 调试和验证
   - 使用objdump检查段的布局
   - 使用nm查看符号表
   - 使用xxd查看二进制内容

## 插件系统设计要点

1. 头部设计
   - 使用固定的魔数和版本号
   - 使用相对偏移量定位函数
   - 保持头部结构简单清晰

2. 编译和链接
   - 使用-ffunction-sections分离函数
   - 使用链接脚本控制布局
   - 使用objcopy生成纯二进制

3. 加载和执行
   - 使用mmap加载插件
   - 验证头部信息
   - 计算函数地址 

## 插件格式优化

### 文件布局优化
1. 头部段（0x00-0x18）
   - Magic: "PPDB" (4字节)
   - Version: 1 (4字节)
   - 函数偏移量 (12字节)
   - 8字节对齐

2. 代码段（0x18-0x40）
   - dl_init、dl_main、dl_fini函数
   - 紧凑布局，无填充
   - 8字节对齐

3. 数据段和BSS段（0x40-）
   - 按需分配
   - 8字节对齐

### 编译优化
1. 编译选项
   ```bat
   -fno-pic -fno-pie -nostdinc
   -ffunction-sections -fdata-sections
   -O0 -fno-inline
   -fno-optimize-sibling-calls
   -fkeep-static-functions
   ```

2. 链接选项
   ```bat
   -static -nostdlib
   --strip-debug
   --section-start=.header=0
   ```

3. 二进制生成
   ```bat
   -O binary
   -R .comment -R .note -R .eh_frame
   -R .rela* -R .gnu* -R .debug*
   ```

## 插件加载测试计划

### 基本功能测试
1. 头部验证
   - Magic和Version检查
   - 偏移量范围检查
   - 段边界检查

2. 内存映射
   - 使用mmap加载
   - 设置rwx权限
   - 验证地址对齐

3. 函数调用
   - 计算函数地址
   - 调用dl_init
   - 调用dl_main
   - 调用dl_fini

### 错误处理
1. 文件错误
   - 文件不存在
   - 文件格式错误
   - 文件权限错误

2. 内存错误
   - 映射失败
   - 权限设置失败
   - 地址越界

3. 执行错误
   - 函数调用失败
   - 返回值检查
   - 异常处理

### 测试用例
1. test11.dl
   - 基本功能测试
   - 返回固定值
   - 验证生命周期

2. 后续测试
   - 添加数据段测试
   - 添加BSS段测试
   - 添加异常测试

// ... existing code ... 