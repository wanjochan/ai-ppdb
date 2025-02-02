#ifndef PEER_MEMKV_H
#define PEER_MEMKV_H

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"

// 声明全局服务实例
extern peer_service_t g_memkv_service;

// 声明服务接口函数
infra_error_t memkv_init(const infra_config_t* config);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
bool memkv_is_running(void);
infra_error_t memkv_cmd_handler(int argc, char** argv);
infra_error_t memkv_load_config(const char* path);
infra_error_t memkv_save_config(const char* path);

#endif /* PEER_MEMKV_H */ 