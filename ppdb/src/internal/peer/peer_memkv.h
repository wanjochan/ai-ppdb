#ifndef PEER_MEMKV_H_
#define PEER_MEMKV_H_

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/poly/poly_db.h"
#include "internal/poly/poly_poll.h"

// 增加缓冲区大小到 1MB
#define MEMKV_CONN_BUFFER_SIZE (1024 * 1024)

// 服务状态
typedef struct {
    char host[64];                    // Host to bind to
    int port;                         // Port to bind to
    char db_path[256];               // Database path
    volatile bool running;            // Service running flag
    infra_mutex_t mutex;              // Service mutex
    poly_poll_context_t* ctx;         // Poll context
} memkv_state_t;

// 连接状态
typedef struct {
    infra_socket_t sock;              // Client socket
    poly_db_t* store;                 // Database connection
    char* rx_buf;                     // Receive buffer
    size_t rx_len;                    // Current buffer length
    bool should_close;                // Connection close flag
    volatile bool is_closing;         // Connection is being destroyed
    volatile bool is_initialized;     // Connection is fully initialized
    uint64_t created_time;           // Connection creation timestamp
    uint64_t last_active_time;       // Last activity timestamp
    size_t total_commands;           // Total commands processed
    size_t failed_commands;          // Failed commands count
    char client_addr[64];            // Client address string
    
    // SET command state
    bool in_set_data;                // Whether processing SET data
    char set_key[256];               // SET command key
    uint32_t set_flags;              // SET command flags
    time_t set_exptime;              // SET command expiration time
    size_t set_bytes;                // SET command data length
    bool set_noreply;                // SET command noreply flag
} memkv_conn_t;

// Service interface functions
infra_error_t memkv_init(void);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
infra_error_t memkv_cmd_handler(const char* cmd, char* response, size_t size);

// Get memkv service instance
peer_service_t* peer_memkv_get_service(void);

#endif /* PEER_MEMKV_H_ */