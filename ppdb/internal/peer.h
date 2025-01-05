#ifndef PPDB_INTERNAL_PEER_H
#define PPDB_INTERNAL_PEER_H

#include "storage.h"

//-----------------------------------------------------------------------------
// Network Types
//-----------------------------------------------------------------------------
typedef struct ppdb_endpoint {
    char host[256];
    uint16_t port;
} ppdb_endpoint_t;

typedef uint64_t ppdb_connection_t;
typedef uint64_t ppdb_server_t;

//-----------------------------------------------------------------------------
// Server Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_server_create(const ppdb_endpoint_t* endpoint, ppdb_server_t* server);
ppdb_error_t ppdb_server_start(ppdb_server_t server);
ppdb_error_t ppdb_server_stop(ppdb_server_t server);
ppdb_error_t ppdb_server_destroy(ppdb_server_t server);

//-----------------------------------------------------------------------------
// Client Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_client_connect(const ppdb_endpoint_t* endpoint, ppdb_connection_t* conn);
ppdb_error_t ppdb_client_disconnect(ppdb_connection_t conn);
ppdb_error_t ppdb_client_put(ppdb_connection_t conn, const ppdb_data_t* key, const ppdb_data_t* value);
ppdb_error_t ppdb_client_get(ppdb_connection_t conn, const ppdb_data_t* key, ppdb_data_t* value);
ppdb_error_t ppdb_client_delete(ppdb_connection_t conn, const ppdb_data_t* key);

//-----------------------------------------------------------------------------
// Cluster Operations
//-----------------------------------------------------------------------------
typedef uint64_t ppdb_cluster_t;

ppdb_error_t ppdb_cluster_init(const char* cluster_id, ppdb_cluster_t* cluster);
ppdb_error_t ppdb_cluster_join(ppdb_cluster_t cluster, const ppdb_endpoint_t* endpoint);
ppdb_error_t ppdb_cluster_leave(ppdb_cluster_t cluster);
ppdb_error_t ppdb_cluster_get_members(ppdb_cluster_t cluster, ppdb_endpoint_t* endpoints, size_t* count);

//-----------------------------------------------------------------------------
// Replication Operations
//-----------------------------------------------------------------------------
typedef uint64_t ppdb_replication_t;

ppdb_error_t ppdb_replication_start(ppdb_cluster_t cluster, ppdb_replication_t* repl);
ppdb_error_t ppdb_replication_stop(ppdb_replication_t repl);
ppdb_error_t ppdb_replication_status(ppdb_replication_t repl, uint32_t* status);

#endif // PPDB_INTERNAL_PEER_H
