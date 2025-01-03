# PPDB 文档索引与全局认知

## 文档导航

### 1. 快速入门
- [系统概览](#系统概览)
- [项目愿景](#项目愿景)
- [核心特性](#核心特性)
- [文档结构](#文档结构)

### 2. 核心文档
- [开发者指南](DEVELOPER_GUIDE.md) - 环境搭建、构建指南、编码规范
- [系统架构](ARCHITECTURE.md) - 系统设计、核心组件、技术选型
- [性能指南](PERFORMANCE_GUIDE.md) - 性能优化、监控指标、最佳实践

## 系统概览

PPDB 是一个跨平台、自组织的分布式去中心化数据库系统，具有以下特点（和目标）：
- 完全去中心化：无需中心节点，所有节点地位平等
- 自组织网络：节点可以动态添加、基于secret自动发现、加入和退出，相当于热拔插
- 跨平台支持：基于 Cosmopolitan 实现多平台兼容
- 高可用性：通过多副本和一致性协议保证数据可靠性，支持资源健康监视

## 系统架构

采用分层架构，包括：
- HTTP API
- 分布式协调层（Raft 共识协议、成员管理）
- 存储引擎（MemTable、WAL、SSTable）
- 文件系统

## 分布式设计
- 使用一致性哈希进行数据分片
- 基于 Raft 实现强一致性
- 支持去中心化的节点发现和自动化的成员管理

## 存储引擎
包含：
- 同步机制
- 跳表实现
- 内存表
- WAL 实现
针对性能进行了多项优化

## 开发路线
分为三个阶段：
1. 单机存储引擎 (MVP)
2. 分布式基础
3. 完整分布式支持
4. 除KV意外加入更多数据形式，如列储存、关系数据库、图数据库等

## 性能目标
- 延迟：读取 P99 < 10ms，写入 P99 < 20ms
- 吞吐量：单节点读取 >100K QPS，写入 >50K QPS
- 可用性：服务可用性 99.99%，数据持久性 99.999999%
