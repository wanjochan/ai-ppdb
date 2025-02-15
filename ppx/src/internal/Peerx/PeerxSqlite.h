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
    infrax_error_t (*open)(PeerxSqlite* self, const peerx_sqlite_conn_info_t* info);
    infrax_error_t (*close)(PeerxSqlite* self);
    infrax_error_t (*exec)(PeerxSqlite* self, const char* sql);
    infrax_error_t (*query)(PeerxSqlite* self, const char* sql, peerx_sqlite_result_t* result);
    void (*free_result)(PeerxSqlite* self, peerx_sqlite_result_t* result);
    
    // Transaction management
    infrax_error_t (*begin)(PeerxSqlite* self);
    infrax_error_t (*commit)(PeerxSqlite* self);
    infrax_error_t (*rollback)(PeerxSqlite* self);
    
    // Backup and restore
    infrax_error_t (*backup)(PeerxSqlite* self, const char* path);
    infrax_error_t (*restore)(PeerxSqlite* self, const char* path);
};

// SQLite service class interface
struct PeerxSqliteClassType {
    // Constructor/Destructor
    PeerxSqlite* (*new)(void);
    void (*free)(PeerxSqlite* self);
    
    // Service lifecycle (inherited from PeerxService)
    infrax_error_t (*init)(PeerxSqlite* self, const polyx_service_config_t* config);
    infrax_error_t (*start)(PeerxSqlite* self);
    infrax_error_t (*stop)(PeerxSqlite* self);
    infrax_error_t (*reload)(PeerxSqlite* self);
    
    // Status and error handling (inherited from PeerxService)
    infrax_error_t (*get_status)(PeerxSqlite* self, char* status, size_t size);
    const char* (*get_error)(PeerxSqlite* self);
    void (*clear_error)(PeerxSqlite* self);
    
    // Configuration (inherited from PeerxService)
    infrax_error_t (*validate_config)(PeerxSqlite* self, const polyx_service_config_t* config);
    infrax_error_t (*apply_config)(PeerxSqlite* self, const polyx_service_config_t* config);
};

// Global class instance
extern const PeerxSqliteClassType PeerxSqliteClass;

#endif // PEERX_SQLITE_INTERFACE_H 