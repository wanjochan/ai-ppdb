#ifndef PEERX_RINETD_INTERFACE_H
#define PEERX_RINETD_INTERFACE_H

#include "PeerxService.h"

// Forward declarations
typedef struct PeerxRinetd PeerxRinetd;
typedef struct PeerxRinetdClassType PeerxRinetdClassType;

// Port forwarding rule
typedef struct {
    char bind_host[POLYX_CMD_MAX_NAME];
    int bind_port;
    char target_host[POLYX_CMD_MAX_NAME];
    int target_port;
    bool enabled;
} peerx_rinetd_rule_t;

// Rinetd service instance
struct PeerxRinetd {
    // Base service
    PeerxService base;
    
    // Rules management
    infrax_error_t (*add_rule)(PeerxRinetd* self, const peerx_rinetd_rule_t* rule);
    infrax_error_t (*remove_rule)(PeerxRinetd* self, const char* bind_host, int bind_port);
    infrax_error_t (*enable_rule)(PeerxRinetd* self, const char* bind_host, int bind_port);
    infrax_error_t (*disable_rule)(PeerxRinetd* self, const char* bind_host, int bind_port);
    infrax_error_t (*get_rules)(PeerxRinetd* self, peerx_rinetd_rule_t* rules, size_t* count);
    
    // Statistics
    infrax_error_t (*get_stats)(PeerxRinetd* self, const char* bind_host, int bind_port, 
                               uint64_t* bytes_in, uint64_t* bytes_out,
                               uint64_t* connections);
};

// Rinetd service class interface
struct PeerxRinetdClassType {
    // Constructor/Destructor
    PeerxRinetd* (*new)(void);
    void (*free)(PeerxRinetd* self);
    
    // Service lifecycle (inherited from PeerxService)
    infrax_error_t (*init)(PeerxRinetd* self, const polyx_service_config_t* config);
    infrax_error_t (*start)(PeerxRinetd* self);
    infrax_error_t (*stop)(PeerxRinetd* self);
    infrax_error_t (*reload)(PeerxRinetd* self);
    
    // Status and error handling (inherited from PeerxService)
    infrax_error_t (*get_status)(PeerxRinetd* self, char* status, size_t size);
    const char* (*get_error)(PeerxRinetd* self);
    void (*clear_error)(PeerxRinetd* self);
    
    // Configuration (inherited from PeerxService)
    infrax_error_t (*validate_config)(PeerxRinetd* self, const polyx_service_config_t* config);
    infrax_error_t (*apply_config)(PeerxRinetd* self, const polyx_service_config_t* config);
};

// Global class instance
extern const PeerxRinetdClassType PeerxRinetdClass;

#endif // PEERX_RINETD_INTERFACE_H 