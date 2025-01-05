#ifndef PPDB_PEER_H
#define PPDB_PEER_H

#include "ppdb.h"

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// Public Network Types
//-----------------------------------------------------------------------------
typedef struct ppdb_server_config {
    const char* host;
    uint16_t port;
    uint32_t max_connections;
    uint32_t timeout_ms;
} ppdb_server_config_t;

typedef struct ppdb_client_config {
    const char* host;
    uint16_t port;
    uint32_t timeout_ms;
    uint32_t retry_count;
} ppdb_client_config_t;

//-----------------------------------------------------------------------------
// Server API
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_server_init(const ppdb_server_config_t* config);
ppdb_error_t ppdb_server_start(void);
ppdb_error_t ppdb_server_stop(void);
ppdb_error_t ppdb_server_cleanup(void);

//-----------------------------------------------------------------------------
// Client API
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_client_init(const ppdb_client_config_t* config);
ppdb_error_t ppdb_client_connect(void);
ppdb_error_t ppdb_client_disconnect(void);
ppdb_error_t ppdb_client_cleanup(void);

// Remote Operations
ppdb_error_t ppdb_remote_put(const void* key, size_t key_size,
                            const void* value, size_t value_size);
ppdb_error_t ppdb_remote_get(const void* key, size_t key_size,
                            void* value, size_t* value_size);
ppdb_error_t ppdb_remote_delete(const void* key, size_t key_size);

//-----------------------------------------------------------------------------
// Cluster API
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_cluster_create(const char* cluster_id);
ppdb_error_t ppdb_cluster_join(const char* host, uint16_t port);
ppdb_error_t ppdb_cluster_leave(void);
ppdb_error_t ppdb_cluster_status(char* status, size_t* status_size);

#ifdef __cplusplus
}
#endif

#endif // PPDB_PEER_H
