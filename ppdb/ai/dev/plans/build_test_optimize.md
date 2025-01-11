# 构建测试脚本优化子计划

## 背景
- 当前build_test_infra.bat只支持全量测试
- 开发过程中经常需要针对单个模块进行测试
- 全量测试耗时较长，影响开发效率
- 需要支持指定模块的测试功能

## 相关文档
- 项目说明：`DESIGN.md`
- 当前脚本：`ppdb/scripts/build_test_infra.bat`

## 工作目标
优化build_test_infra.bat，实现：
1. 保持原有全量测试功能
2. 增加指定模块测试功能
3. 确保向后兼容性
4. 优化输出信息展示

## 工作内容

### 1. 脚本分析
- 分析当前build_test_infra.bat的实现逻辑
- 确定参数传递方式
- 识别测试目录结构（ppdb/test/white/infra/）
- 分析编译和链接过程

### 2. 功能设计
- 参数格式：build_test_infra.bat [module_name]
- 不带参数时执行全量测试（当前行为）
- 带参数时只测试指定模块（如memory、string等）
- 模块名称验证（检查对应的test_{module}.c是否存在）

### 3. 实现修改
- 添加参数解析逻辑
- 实现模块名称验证
- 修改编译和链接命令
- 优化输出信息格式

### 4. 测试验证
- 验证全量测试功能（原有功能）
- 验证单模块测试（如memory模块）
- 验证错误参数处理
- 验证输出信息正确性

## 工作指引

### 开发过程
1. 先备份原脚本
2. 小步修改，逐步测试
3. 保持脚本的可读性
4. 添加必要的注释

### 日志记录
在`ppdb/ai/dev/logs/build_test_optimize.log`记录：
- 脚本分析结果
- 修改内容说明
- 测试执行结果
- 发现的问题和解决方案

### 状态更新
在`ppdb/ai/dev/status/build_test_optimize.json`更新：
- 当前进展情况
- 完成的修改项
- 待处理的工作
- 测试结果数据

### 使用示例
```batch
:: 全量测试（保持原有行为）
build_test_infra.bat

:: 测试指定模块
build_test_infra.bat memory
build_test_infra.bat string
```

## 完成标准
1. 脚本功能完整实现
2. 向后兼容性验证通过
3. 所有测试场景验证通过
4. 输出信息清晰规范
5. 脚本注释完整
6. 更新DESIGN.md中的构建说明部分 