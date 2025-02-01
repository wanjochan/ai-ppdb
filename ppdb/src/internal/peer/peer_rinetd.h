#ifndef PEER_RINETD_H
#define PEER_RINETD_H

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"

// 声明全局服务实例
extern peer_service_t g_rinetd_service;

// 声明服务接口函数
infra_error_t rinetd_init(const infra_config_t* config);
infra_error_t rinetd_cleanup(void);
infra_error_t rinetd_start(void);
infra_error_t rinetd_stop(void);
bool rinetd_is_running(void);
infra_error_t rinetd_cmd_handler(int argc, char** argv);
infra_error_t rinetd_load_config(const char* path);
infra_error_t rinetd_save_config(const char* path);

#endif /* PEER_RINETD_H */ 