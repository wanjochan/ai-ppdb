# 严格按以下顺序进行 memkv 测试
  - 如果测试出现问题，不要着急改，要深度分析，分析出问题的原因，再根据原因进行修改 peer_memkv.c 文件
  - 如果改动了 peer_memkv.c 哪怕一点点也必须要严格按这个顺序重新来测试!
  - 如果编译不通过就恢复 peer_memkv.c 文件，重新再来!
  - 如果修复了任何一个测试，也要重头再来按顺序测试（因为有一定的依赖关系）
```
## 先是分步测试
sh ppdb/scripts/test_memkv.sh -k test_basic_set_get
sh ppdb/scripts/test_memkv.sh -k test_delete
sh ppdb/scripts/test_memkv.sh -k test_not_found
sh ppdb/scripts/test_memkv.sh -k test_multi_set_get
sh ppdb/scripts/test_memkv.sh -k test_expiration
sh ppdb/scripts/test_memkv.sh -k test_increment_decrement
sh ppdb/scripts/test_memkv.sh -k test_large_value
## 最后整体测试（目前未通过，要小心分析）：
sh ppdb/scripts/test_memkv.sh
```

## 问题描述
- 需要重新测试 memkv 功能
- 之前的修改导致了问题，现已恢复代码
- 需要按特定顺序执行测试用例

## 分析
1. 测试顺序（从文档获取）：
   - test_basic_set_get
   - test_delete
   - test_not_found
   - test_multi_set_get
   - test_expiration
   - test_increment_decrement
   - test_large_value
   - 最后进行整体测试

2. 关键点：
   - 每个测试都需要独立验证
   - 发现问题时需要立即停止并分析
   - 避免在未完全理解问题前修改代码

## 执行计划
1. 准备阶段
   - 确认当前代码状态
   - 检查测试环境

2. 测试阶段
   - 按顺序逐个执行测试
   - 每个测试后记录结果
   - 如遇问题，立即停止并分析

3. 问题处理
   - 详细记录每个失败的测试
   - 分析日志和错误信息
   - 在理解问题后再提出修改方案

## 当前状态
准备开始执行测试计划 