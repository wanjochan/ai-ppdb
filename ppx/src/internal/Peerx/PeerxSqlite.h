#ifndef PEERX_SQLITE_INTERFACE_H
#define PEERX_SQLITE_INTERFACE_H

#include "PeerxService.h"

// Forward declarations
typedef struct PeerxSqlite PeerxSqlite;
typedef struct PeerxSqliteClassType PeerxSqliteClassType;

// Database connection info
typedef struct {
    char path[POLYX_CMD_MAX_VALUE];
    bool read_only;
    bool in_memory;
    int timeout_ms;
} peerx_sqlite_conn_info_t;

// Query result
typedef struct {
    int column_count;
    char** column_names;
    char*** rows;
    int row_count;
} peerx_sqlite_result_t;

// SQLite service instance
struct PeerxSqlite {
    // Base service
    PeerxService base;
    
    // Database operations
    InfraxError (*open)(PeerxSqlite* self, const peerx_sqlite_conn_info_t* info);
    InfraxError (*close)(PeerxSqlite* self);
    InfraxError (*exec)(PeerxSqlite* self, const char* sql);
    InfraxError (*query)(PeerxSqlite* self, const char* sql, peerx_sqlite_result_t* result);
    void (*free_result)(PeerxSqlite* self, peerx_sqlite_result_t* result);
    
    // Transaction management
    InfraxError (*begin)(PeerxSqlite* self);
    InfraxError (*commit)(PeerxSqlite* self);
    InfraxError (*rollback)(PeerxSqlite* self);
    
    // Backup and restore
    InfraxError (*backup)(PeerxSqlite* self, const char* path);
    InfraxError (*restore)(PeerxSqlite* self, const char* path);
};

// SQLite service class interface
struct PeerxSqliteClassType {
    // Constructor/Destructor
    PeerxSqlite* (*new)(void);
    void (*free)(PeerxSqlite* self);
    
    // Service lifecycle (inherited from PeerxService)
    InfraxError (*init)(PeerxSqlite* self, const polyx_service_config_t* config);
    InfraxError (*start)(PeerxSqlite* self);
    InfraxError (*stop)(PeerxSqlite* self);
    InfraxError (*reload)(PeerxSqlite* self);
    
    // Status and error handling (inherited from PeerxService)
    InfraxError (*get_status)(PeerxSqlite* self, char* status, size_t size);
    const char* (*get_error)(PeerxSqlite* self);
    void (*clear_error)(PeerxSqlite* self);
    
    // Configuration (inherited from PeerxService)
    InfraxError (*validate_config)(PeerxSqlite* self, const polyx_service_config_t* config);
    InfraxError (*apply_config)(PeerxSqlite* self, const polyx_service_config_t* config);
};

// Global class instance
extern const PeerxSqliteClassType PeerxSqliteClass;

#endif // PEERX_SQLITE_INTERFACE_H 