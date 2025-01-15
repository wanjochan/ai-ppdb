# Cosmo动态加载器

## 项目目标

利用Cosmopolitan工具链，实现一个轻量级的动态加载器，用于加载和执行特定格式的插件文件。该项目是PPDB（跨平台动态库模块）的一个组成部分。

### 核心目标
1. 实现简单的插件格式和加载机制
2. 插件文件包含完整的静态编译代码（包含cosmopolitan库）
3. 支持标准的生命周期管理（init/main/fini）
4. 确保与Cosmopolitan工具链的兼容性

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

## 开发规范

### 插件开发规范
1. 命名规范
   - 插件文件后缀统一使用`.dl`
   - 导出函数必须使用`dl_`前缀（如`dl_init`、`dl_main`、`dl_fini`）
   - 内部函数使用`static`修饰，避免符号冲突
   - 变量和函数使用小写字母和下划线命名

2. 函数导出规范
   ```c
   /* 必须使用这两个属性 */
   __attribute__((section(".text.dl_name")))
   __attribute__((used))
   ```
   - 必须导出的函数：
     * `dl_init`：初始化函数，返回0表示成功
     * `dl_main`：主函数，返回值由插件定义
     * `dl_fini`：清理函数，返回0表示成功

3. 内存使用规范
   - 避免使用全局变量，优先使用函数参数传递数据
   - 必要的全局变量放在`.data`或`.bss`段
   - 字符串常量放在`.rodata`段
   - 遵循8字节对齐原则

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


## 开发经验总结

### 链接脚本设计
1. 段的保留和丢弃
   - 不能丢弃`.ape*`段，这些段包含了Cosmopolitan运行时所需的代码
   - 可以安全丢弃的段：`.comment`、`.note*`、`.debug*`等
   - 需要保留的关键段：`.text*`、`.data*`、`.bss*`、`.init*`

2. 函数导出
   - 使用`__attribute__((section(".text.NAME")))`指定函数段
   - 使用`__attribute__((used))`防止函数被优化掉
   - 使用`KEEP()`指令在链接时保留指定段

3. 内存布局
   - 从地址0开始布局，便于计算偏移量
   - 使用`ALIGN(8)`确保段对齐
   - 使用`PHDRS`定义段的权限（r/w/x）

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