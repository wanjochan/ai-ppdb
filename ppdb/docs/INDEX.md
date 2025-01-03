# PPDB 文档索引

> 欢迎来到 PPDB 文档中心。本文档提供了 PPDB 项目的完整文档索引，帮助你快速找到所需信息。

## 1. 核心文档

### 1.1 概览文档
- [系统概览](overview/OVERVIEW.md)
  * 项目愿景和核心特性
  * 系统架构概述
  * 文档导航
  * 开发路线

### 1.2 开发文档
- [开发者指南](overview/DEVELOPER_GUIDE.md)
  * 环境配置和构建步骤
  * 编码规范
  * 错误处理
  * 测试指南
  * 调试技巧

### 1.3 技术文档
- [系统架构](overview/ARCHITECTURE.md)
  * 架构设计
  * 核心组件
  * 技术选型
  * 数据流设计
  * 可靠性保证

### 1.4 性能文档
- [性能优化指南](overview/PERFORMANCE_GUIDE.md)
  * 性能基准
  * 优化策略
  * 监控指标
  * 调优指南
  * 问题诊断

### 1.5 设计文档
- [同步原语设计](design/sync.md)
  * 基础同步原语
  * 数据库同步层
  * 无锁算法实现
  * 性能优化策略
- [无锁数据结构](design/lockfree.md)
- [预写日志(WAL)](design/wal.md)
- [存储引擎](design/storage.md)
- [SSTable 实现](design/sstable.md)
- [内存表设计](design/memtable.md)
- [成员管理](design/membership.md)

### 1.6 API 文档
- [存储接口](api/STORAGE.md)
- [监控接口](api/MONITOR.md)
- [命令行工具](api/CLI.md)

### 1.7 测试文档
- [测试计划](test/PLAN.md)
- [性能测试](test/PERFORMANCE.md)
- [测试框架](test/FRAMEWORK.md)
- [测试用例](test/CASES.md)

## 2. 快速入门

### 2.1 新手上路
1. 阅读 [系统概览](overview/OVERVIEW.md) 了解项目
2. 按照 [开发者指南](overview/DEVELOPER_GUIDE.md) 搭建环境
3. 参考 [系统架构](overview/ARCHITECTURE.md) 理解设计
4. 查看 [性能优化指南](overview/PERFORMANCE_GUIDE.md) 了解性能特性

### 2.2 常见问题
- 环境配置问题：参考 [开发者指南 - 环境配置](overview/DEVELOPER_GUIDE.md#2-环境配置)
- 构建问题：参考 [开发者指南 - 构建指南](overview/DEVELOPER_GUIDE.md#3-构建指南)
- 性能问题：参考 [性能优化指南 - 问题诊断](overview/PERFORMANCE_GUIDE.md#5-性能问题诊断)

## 3. 学习资料
- [共识算法](learn/consensus.md)
- [分布式系统基础](learn/distributed.md)
- [存储引擎学习](learn/storage_engine_study.md)
- [分布式模式](learn/distributed_patterns.md)
- [智能学习过程](learn/intelligent_learning_process.md)
- [分布式数据库研究](learn/distributed_db_study.md)

## 4. 文档约定

### 4.1 文档规范
- 所有文档使用 Markdown 格式
- 代码示例需要包含完整注释
- 技术名词保持一致性
- 版本号遵循语义化版本规范

### 4.2 文档维护
- 定期审查和更新
- 保持与代码的同步
- 及时响应反馈
- 记录重要变更
