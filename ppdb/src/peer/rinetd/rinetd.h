#ifndef RINETD_H
#define RINETD_H

#include "cosmopolitan.h"
#include "internal/infra/infra.h"

// 转发规则配置
typedef struct {
    char* listen_addr;     // 监听地址
    uint16_t listen_port;  // 监听端口
    char* forward_addr;    // 转发地址
    uint16_t forward_port; // 转发端口
    bool enabled;          // 是否启用
} rinetd_rule_t;

// 转发服务配置
typedef struct {
    size_t max_connections;    // 最大连接数
    size_t buffer_size;        // 缓冲区大小
    uint32_t timeout;          // 超时时间(毫秒)
} rinetd_config_t;

// 转发服务句柄
typedef struct rinetd_server_t rinetd_server_t;

// 初始化转发服务
infra_error_t rinetd_init(const rinetd_config_t* config, rinetd_server_t** server);

// 添加转发规则
infra_error_t rinetd_add_rule(rinetd_server_t* server, const rinetd_rule_t* rule);

// 启动转发服务
infra_error_t rinetd_start(rinetd_server_t* server);

// 停止转发服务
infra_error_t rinetd_stop(rinetd_server_t* server);

// 清理转发服务
void rinetd_cleanup(rinetd_server_t* server);

#endif /* RINETD_H */ 