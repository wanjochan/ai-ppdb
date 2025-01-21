# Linux SO (Shared Object) 文件格式

## ELF 共享库特性
### 位置无关代码 (PIC)
- GOT (Global Offset Table)
- PLT (Procedure Linkage Table)
- 重定位

### 符号版本控制
- 符号版本
- 版本脚本
- soname 机制

## 动态链接
### 运行时加载
- dlopen/dlsym
- 依赖处理
- 符号解析

### 链接优化
- 延迟绑定
- 预加载
- 符号可见性

## 调试支持
- 调试符号
- 堆栈回溯
- 运行时检查
