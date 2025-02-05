#ifndef PEER_SQLITE3_H
#define PEER_SQLITE3_H

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"

// Service state
typedef struct {
    char db_path[256];                // Database path
    infra_socket_t listener;          // Listener socket
    volatile bool running;            // Service running flag
    infra_mutex_t mutex;              // Service mutex
    void* poll_ctx;                   // Poll context
} sqlite3_service_t;

// 声明全局服务实例
extern peer_service_t g_sqlite3_service;

// 声明服务接口函数
infra_error_t sqlite3_init(void);
infra_error_t sqlite3_cleanup(void);
infra_error_t sqlite3_start(void);
infra_error_t sqlite3_stop(void);
infra_error_t sqlite3_cmd_handler(const char* cmd, char* response, size_t size);

#endif /* PEER_SQLITE3_H */