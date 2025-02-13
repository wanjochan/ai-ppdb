# PolyxAsync Poll场景扩展方案

## 问题分析
1. 现有代码已支持基础事件类型：
   - POLYX_EVENT_NONE
   - POLYX_EVENT_IO
   - POLYX_EVENT_TIMER
   - POLYX_EVENT_SIGNAL

2. poll()机制支持但尚未实现的场景：
   - 网络相关：TCP、UDP、Unix domain sockets
   - 标准IO：管道、FIFO、终端设备
   - 其他：字符设备、inotify等

## 已完成的扩展
1. 新增事件类型：
   - 网络相关：TCP、UDP、Unix domain sockets
   - 标准IO：管道、FIFO、终端设备
   - 其他：inotify、字符设备

2. 新增配置结构：
   - PolyxNetworkConfig：网络事件配置
   - PolyxIOConfig：IO事件配置
   - PolyxInotifyConfig：文件监控配置

3. 扩展PolyxEvent结构：
   - 使用union存储不同类型事件的数据
   - 添加辅助宏和函数简化使用

4. 扩展类接口：
   - 网络事件创建方法
   - IO事件创建方法
   - 文件监控方法

## 下一步计划
1. 实现.c文件中的具体功能：
   - 网络事件处理实现
   - IO事件处理实现
   - 文件监控实现

2. 添加测试用例：
   - 网络事件测试
   - IO事件测试
   - 文件监控测试

3. 文档完善：
   - 添加新接口使用说明
   - 添加示例代码

## 注意事项
1. 保持接口简洁性
2. 确保内存管理一致性
3. 考虑跨平台兼容性
4. 需要同步修改相关测试用例

## 遗留问题
1. 是否需要添加更多辅助函数
2. 是否需要为不同类型事件添加更多特定配置
3. 是否需要添加事件状态查询接口 