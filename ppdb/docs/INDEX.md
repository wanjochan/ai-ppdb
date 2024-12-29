# PPDB 文档索引

## 概述
PPDB (Peer-to-Peer Distributed Database) 是一个跨平台、自组织的分布式去中心化数据库系统。本文档提供了项目的完整索引。

## 重要说明
智能助理必须遵循以下工作流程：
1. 每次会话开始时，必须先阅读 [开发计划](PLAN.md) 了解当前进展
2. 在工作过程中，实时更新相关文档
3. 每次会话结束时，确保所有更改都已记录在文档中

## 文档结构

### 1. 项目概览
- [开发计划](PLAN.md) - 项目整体规划和任务管理 **(主要工作文档)**
- [设计概览](overview/DESIGN.md) - 系统整体设计和架构
- [开发指南](overview/DEVELOPMENT.md) - 开发环境配置、编码规范和错误处理指南
- [构建指南](overview/BUILD.md) - 详细的构建环境、步骤和选项说明
- [重构计划](overview/REFACTOR.md) - 代码重构和改进计划
- [开发经验](overview/EXPERIENCE.md) - 项目开发过程中的经验总结
- [性能报告](overview/PERFORMANCE.md) - 性能测试基准和优化指南

### 2. 详细设计
- [分布式系统设计](design/distributed.md) - 分布式架构设计
- [共识协议实现](design/consensus.md) - Raft 共识协议实现
- [成员管理设计](design/membership.md) - 节点发现和成员管理
- [MemTable 设计](design/memtable.md) - 内存表实现
- [WAL 设计](design/wal.md) - Write-Ahead Log 实现
- [SSTable 设计](design/sstable.md) - 持久化存储格式
- [无锁实现设计](design/lockfree.md) - 无锁数据结构和并发控制实现

### 3. API 文档
- [命令行接口](api/CLI.md) - 命令行工具使用指南
- [HTTP API](api/HTTP.md) - HTTP 接口文档
- [存储引擎 API](api/STORAGE.md) - 存储引擎接口文档

### 4. 测试文档
- [测试计划](test/PLAN.md) - 整体测试策略和计划
- [测试框架](test/FRAMEWORK.md) - 测试框架设计和规范
- [测试用例](test/CASES.md) - 详细测试用例集
- [性能测试](test/PERFORMANCE.md) - 性能测试计划和指标

### 5. 运维文档
- [部署指南](ops/DEPLOY.md) - 部署和配置指南
- [监控方案](ops/MONITOR.md) - 监控和告警方案
- [运维手册](ops/MAINTAIN.md) - 日常运维手册
- [故障恢复](ops/RECOVERY.md) - 故障处理和恢复流程

### 6. 学习资料
- [分布式模式](learn/distributed_patterns.md) - 分布式系统通用设计模式研究
- [分布式数据库研究](learn/distributed_db_study.md) - HBase和Cassandra等系统研究
- [存储引擎研究](learn/storage_engine_study.md) - 存储引擎技术研究
- [设计决策](learn/design_decisions.md) - 重要设计决策记录
- [学习流程](learn/intelligent_learning_process.md) - 智能化学习方法论
- [迭代计划](learn/current_iteration_plan.md) - 当前学习迭代计划

## 文档关系说明

### 入门路径
1. 新人先阅读：
   - `overview/DESIGN.md` - 了解系统整体架构
   - `overview/DEVELOPMENT.md` - 掌握开发环境和规范
   - `overview/BUILD.md` - 学习如何构建项目

2. 开发者重点关注：
   - `PLAN.md` - 了解当前任务和计划
   - `design/` 目录 - 深入理解技术实现
   - `api/` 目录 - 掌握接口使用

3. 运维人员重点关注：
   - `ops/` 目录 - 运维相关文档
   - `test/PERFORMANCE.md` - 性能测试指标

### 文档依赖关系
1. 设计文档关系：
   - `overview/DESIGN.md` 是整体设计，其他 `design/` 目录下文档是各个模块的详细设计
   - `design/distributed.md` 是分布式架构的总体设计，`consensus.md` 和 `membership.md` 是其中的具体实现
   - `design/memtable.md`、`wal.md` 和 `sstable.md` 构成存储引擎的三个核心组件

2. 开发文档关系：
   - `overview/DEVELOPMENT.md` 和 `BUILD.md` 互补，前者关注开发规范，后者关注构建过程
   - `overview/REFACTOR.md` 指导代码改进，与 `DEVELOPMENT.md` 配合使用

3. 学习文档关系：
   - `learn/distributed_patterns.md` 提供理论基础，`distributed_db_study.md` 提供实践参考
   - `learn/intelligent_learning_process.md` 指导整体学习方法，`current_iteration_plan.md` 是具体实施计划

## 文档更新指南

1. 新增文档
   - 在对应目录创建文档
   - 更新本索引文件
   - 确保文档间的链接正确

2. 文档规范
   - 使用 Markdown 格式
   - 文件名使用大写
   - 包含适当的目录结构
   - 保持文档的一致性

3. 文档审查
   - 技术准确性
   - 文档完整性
   - 链接有效性
