#ifndef PEER_SQLITE3_H
#define PEER_SQLITE3_H

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"

// 声明全局服务实例
extern peer_service_t g_sqlite3_service;

// 声明服务接口函数
infra_error_t sqlite3_init(const infra_config_t* config);
infra_error_t sqlite3_cleanup(void);
infra_error_t sqlite3_start(void);
infra_error_t sqlite3_stop(void);
bool sqlite3_is_running(void);
infra_error_t sqlite3_cmd_handler(int argc, char** argv);
infra_error_t sqlite3_load_config(const char* path);
infra_error_t sqlite3_save_config(const char* path);

#endif /* PEER_SQLITE3_H */ 