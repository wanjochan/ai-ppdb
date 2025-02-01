#ifndef PEER_SERVICE_H
#define PEER_SERVICE_H

#include "internal/infra/infra_core.h"
#include "internal/poly/poly_cmdline.h"

//-----------------------------------------------------------------------------
// Service Interface
//-----------------------------------------------------------------------------

// 服务类型
typedef enum {
    SERVICE_TYPE_UNKNOWN = 0,
    SERVICE_TYPE_MEMKV,
    SERVICE_TYPE_RINETD,
    SERVICE_TYPE_SQLITE3,
    SERVICE_TYPE_COUNT
} peer_service_type_t;

// 服务状态
typedef enum {
    SERVICE_STATE_UNKNOWN = 0,  // 未知状态
    SERVICE_STATE_STOPPED,      // 已停止
    SERVICE_STATE_STARTING,     // 正在启动
    SERVICE_STATE_RUNNING,      // 正在运行
    SERVICE_STATE_STOPPING      // 正在停止
} peer_service_state_t;

// 服务配置
typedef struct {
    const char* name;                // 服务名称
    peer_service_type_t type;        // 服务类型
    const poly_cmd_option_t* options;  // 命令行选项
    int option_count;                // 选项数量
    infra_config_t* config;          // 基础配置
    const char* config_path;         // 配置文件路径
} peer_service_config_t;

// 服务接口
typedef struct {
    // 服务配置
    peer_service_config_t config;
    
    // 服务状态
    peer_service_state_t state;
    
    // 生命周期管理
    infra_error_t (*init)(const infra_config_t* config);
    infra_error_t (*cleanup)(void);
    infra_error_t (*start)(void);
    infra_error_t (*stop)(void);
    bool (*is_running)(void);
    
    // 命令行处理
    infra_error_t (*cmd_handler)(int argc, char** argv);
} peer_service_t;

//-----------------------------------------------------------------------------
// Service Registry
//-----------------------------------------------------------------------------

// 注册服务
infra_error_t peer_service_register(peer_service_t* service);

// 通过类型获取服务
peer_service_t* peer_service_get_by_type(peer_service_type_t type);

// 通过名称获取服务
peer_service_t* peer_service_get(const char* name);

// 获取服务名称
const char* peer_service_get_name(peer_service_type_t type);

// 获取服务状态
peer_service_state_t peer_service_get_state(peer_service_type_t type);

#endif // PEER_SERVICE_H 