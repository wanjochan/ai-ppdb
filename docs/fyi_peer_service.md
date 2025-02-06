给 ppx 参考（ppdb 不再改太多了，在 ppx重构补做吧）

# peer_service 模块抽象设计建议

## 1. 基础服务接口

基础服务结构设计：

```c
// 基础服务配置
typedef struct {
    char name[64];              // 服务名称
    void* user_data;           // 用户数据
    poly_poll_context_t* poll_ctx;  // 轮询上下文(如果是网络服务)
    infra_mutex_t mutex;        // 服务互斥锁
    bool running;               // 运行状态
} peer_service_base_t;

// 服务生命周期回调接口
typedef struct {
    infra_error_t (*init)(peer_service_base_t* base);
    infra_error_t (*cleanup)(peer_service_base_t* base);
    infra_error_t (*start)(peer_service_base_t* base);
    infra_error_t (*stop)(peer_service_base_t* base);
    infra_error_t (*apply_config)(peer_service_base_t* base, const poly_service_config_t* config);
    infra_error_t (*handle_cmd)(peer_service_base_t* base, const char* cmd, char* response, size_t size);
} peer_service_ops_t;

// 完整服务结构
typedef struct {
    peer_service_base_t base;      // 基础服务状态
    peer_service_ops_t ops;        // 服务操作接口
    peer_service_state_t state;    // 服务状态
} peer_service_t;
```

## 2. 网络服务扩展

针对网络服务的特殊需求：

```c
// 网络服务配置
typedef struct {
    peer_service_base_t base;     // 继承基础服务
    int min_threads;              // 最小线程数
    int max_threads;              // 最大线程数
    int queue_size;               // 队列大小
    void (*connection_handler)(void* args);  // 连接处理回调
} peer_network_service_t;

// 网络服务通用操作
infra_error_t peer_network_service_init(peer_network_service_t* svc);
infra_error_t peer_network_service_start(peer_network_service_t* svc);
infra_error_t peer_network_service_stop(peer_network_service_t* svc);
infra_error_t peer_network_service_add_listener(peer_network_service_t* svc, 
    const char* addr, int port, void* user_data);
```

## 3. 命令处理扩展

统一的命令处理机制：

```c
// 通用命令处理
typedef struct {
    const char* name;           // 命令名称
    const char* description;    // 命令描述
    infra_error_t (*handler)(peer_service_base_t* base, int argc, char** argv);
} peer_service_command_t;

// 命令注册和处理
infra_error_t peer_service_register_command(peer_service_t* svc, 
    const peer_service_command_t* cmd);
infra_error_t peer_service_handle_command(peer_service_t* svc, 
    const char* cmd_line, char* response, size_t size);
```

## 4. 配置管理扩展

配置管理接口：

```c
// 配置验证和应用
typedef struct {
    infra_error_t (*validate)(const poly_service_config_t* config);
    infra_error_t (*apply)(peer_service_base_t* base, const poly_service_config_t* config);
} peer_service_config_ops_t;
```

## 5. 设计优势

1. **灵活性**：服务可以选择性地实现需要的接口
2. **可扩展**：基于基础服务可以派生出网络服务、命令行工具等不同类型
3. **代码复用**：通用的生命周期管理、状态转换等逻辑可以在基类中实现
4. **统一接口**：所有服务都遵循相同的接口约定

## 6. 具体服务实现指南

具体服务（如 rinetd、sqlite3、memkv、diskv、fetcher、cdp、cli 等）只需要：

1. 继承基础服务结构
2. 实现必要的回调接口
3. 注册自己的命令
4. 专注于业务逻辑实现

## 7. 代码量优化

通过这种抽象，可以：
1. 减少每个服务模块约 25% 的代码量
2. 统一错误处理和日志记录
3. 简化新服务的开发流程
4. 提高代码可维护性

