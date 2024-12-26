# PPDB 文档索引

## 概述
PPDB (Peer-to-Peer Distributed Database) 是一个跨平台、自组织的分布式去中心化数据库系统。本文档提供了项目的完整索引。

## 重要说明
智能助理必须遵循以下工作流程：
1. 每次会话开始时，必须先阅读 [开发计划](overview/TODO.md) 了解当前进展
2. 在工作过程中，实时更新相关文档
3. 每次会话结束时，确保所有更改都已记录在文档中

## 文档结构

### 1. 项目概览
- [开发计划](overview/TODO.md) - 项目进展追踪和任务管理 **(智能助理工作主文档)**
- [设计概览](overview/DESIGN.md) - 系统整体设计和架构
- [开发指南](overview/DEVELOPMENT.md) - 开发环境配置和规范
- [开发经验](overview/EXPERIENCE.md) - 项目开发过程中的经验总结

### 2. 详细设计
- [分布式系统设计](design/distributed.md) - 分布式架构设计
- [共识协议实现](design/consensus.md) - Raft 共识协议实现
- [成员管理设计](design/membership.md) - 节点发现和成员管理
- [MemTable 设计](design/memtable.md) - 内存表实现
- [WAL 设计](design/wal.md) - Write-Ahead Log 实现
- [SSTable 设计](design/sstable.md) - 持久化存储格式

### 3. API 文档
- [HTTP API](api/HTTP.md) - HTTP 接口文档
- [存储引擎 API](api/STORAGE.md) - 存储引擎接口文档

### 4. 测试文档
- [测试计划](test/PLAN.md) - 整体测试策略和计划
- [测试框架](test/FRAMEWORK.md) - 测试框架设计和规范
- [测试用例](test/CASES.md) - 详细测试用例集

### 5. 运维文档
- [部署指南](ops/DEPLOY.md) - 部署和配置指南
- [监控方案](ops/MONITOR.md) - 监控和告警方案
- [运维手册](ops/MAINTAIN.md) - 日常运维手册

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
