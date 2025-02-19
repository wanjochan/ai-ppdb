# macOS DYLD (Dynamic Loader) 和 DYLIB 文件格式

## Mach-O 文件结构
- Header
- Load Commands
- Segments 和 Sections

## DYLD 特性
### 两级命名空间
- 库标识
- 版本控制

### 加载过程
- DYLD 缓存
- 依赖解析
- 符号绑定

## 重定位
- 非 PIE 重定位
- 动态符号重定位

## 优化机制
- DYLD 共享缓存
- 启动时优化
- 绑定和重绑定
