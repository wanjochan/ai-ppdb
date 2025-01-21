# Windows DLL (Dynamic Link Library) 文件格式

## PE 文件结构
- DOS Header 和 DOS Stub
- PE Header
- Section Headers
- 导入表和导出表

## 关键组件
### 导出表
- 函数名称
- 序号
- 函数地址

### 导入表
- DLL 名称
- 函数名称
- IAT (Import Address Table)

### 重定位表
- 基址重定位
- 修正地址

## 加载机制
- LoadLibrary 流程
- DLL 搜索顺序
- 依赖处理

## 特殊功能
- DllMain 入口点
- 延迟加载
- TLS (Thread Local Storage)
