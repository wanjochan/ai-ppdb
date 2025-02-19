# PPX Latest 构建任务跟踪

···
现在 Infrax 层清理完了。 Polyx 和 Peerx 和 ppx.c 都参考 ppdb/的做了简单的迁移，
是时候编译 ppx_latest.exe 并调整 Polyx/Peerx 了。
要一步步来，先执行
sh ppx/scripts/build_ppx.sh
分析讨论问题（先不要修改文件）
相应不要改 Infrax 层，如果发现 Infrax 层有问题，先分析原因，等我通知再修改。
···

## 当前状态
- Infrax 层已完成清理（不可修改）
- Polyx/Peerx/ppx.c 已参考 ppdb 完成初步迁移
- 准备构建 ppx_latest.exe

## 计划步骤
1. 检查构建脚本
2. 执行构建
3. 分析问题
4. 制定解决方案

## 执行记录

### 2024-03-21 构建尝试
执行 `sh ppx/scripts/build_ppx.sh` 遇到编译错误。

### 2024-03-21 深入分析（修正版）

检查了 InfraxCore.h 和 PolyxCmdline.h 的内容，发现以下问题：

1. Polyx 层使用了不兼容的类型和函数：
   - 使用了系统类型（bool, size_t 等）而不是 InfraxBool, InfraxSize
   - 直接使用了系统函数（printf/fprintf）而不是 InfraxCore 的方法
   - 错误处理使用了 infrax_error_t 而不是 InfraxError

2. 缺少必要的实现：
   - PolyxCmdline 的类成员函数未实现
   - PolyxServiceCmd 的类成员函数未实现

3. 架构问题：
   - Polyx 层未正确使用 InfraxCore 实例
   - 系统调用未通过 InfraxCore 进行封装

### 2024-03-21 修改进展

1. 已完成的修改：
   - 修改了 PolyxCmdline.h，使用 Infrax 类型（InfraxBool, InfraxSize, InfraxError 等）
   - 修改了 ppx.c，使用 InfraxCore 的方法（printf 等）
   - 修改了 PolyxConfig.h，使用 InfraxError 替换 infrax_error_t
   - 修改了 PolyxService.h，使用 InfraxError 和 InfraxSize
   - 修改了 PolyxServiceCmd.h，使用 InfraxError 和 InfraxI32
   - 修改了 PolyxCmdline.c，使用 Infrax 类型和方法
   - 移除了对系统函数的直接调用
   - 统一使用 make_error 和 INFRAX_ERROR_IS_OK 进行错误处理
   - 实现了 string_to_int 函数替代 atoi
   - 使用 INFRAX_ERROR_FILE_NOT_FOUND 替代 INFRAX_ERROR_NOT_FOUND

2. 待完成的工作：
   - 实现 PolyxServiceCmd.c
   - 测试所有功能

## 下一步计划

1. 实现 PolyxServiceCmd.c：
   a. 创建文件
   b. 实现所有类成员函数
   c. 确保使用 Infrax 类型和方法

2. 测试计划：
   - 编译测试
   - 基本功能测试
   - 错误处理测试
   - 内存泄漏测试 