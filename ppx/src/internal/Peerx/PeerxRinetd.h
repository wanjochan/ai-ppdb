#ifndef PEERX_RINETD_INTERFACE_H
#define PEERX_RINETD_INTERFACE_H

#include "PeerxService.h"
#include "internal/polyx/PolyxCmdline.h"
#include "internal/polyx/PolyxConfig.h"

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
};

// Rinetd service class interface
struct PeerxRinetdClassType {
    // Constructor/Destructor
    PeerxRinetd* (*new)(void);
    void (*free)(PeerxRinetd* self);
    
    // Service lifecycle (inherited from PeerxService)
    InfraxError (*init)(PeerxRinetd* self, const polyx_service_config_t* config);
    InfraxError (*start)(PeerxRinetd* self);
    InfraxError (*stop)(PeerxRinetd* self);
    InfraxError (*reload)(PeerxRinetd* self);
    
    // Status and error handling (inherited from PeerxService)
    InfraxError (*get_status)(PeerxRinetd* self, char* status, size_t size);
    const char* (*get_error)(PeerxRinetd* self);
    void (*clear_error)(PeerxRinetd* self);
    
    // Configuration (inherited from PeerxService)
    InfraxError (*validate_config)(PeerxRinetd* self, const polyx_service_config_t* config);
    InfraxError (*apply_config)(PeerxRinetd* self, const polyx_service_config_t* config);

    // Rules management
    InfraxError (*add_rule)(PeerxRinetd* self, const peerx_rinetd_rule_t* rule);
    InfraxError (*remove_rule)(PeerxRinetd* self, const char* bind_host, int bind_port);
    InfraxError (*enable_rule)(PeerxRinetd* self, const char* bind_host, int bind_port);
    InfraxError (*disable_rule)(PeerxRinetd* self, const char* bind_host, int bind_port);
    InfraxError (*get_rules)(PeerxRinetd* self, peerx_rinetd_rule_t* rules, size_t* count);
    
    // Statistics
    InfraxError (*get_stats)(PeerxRinetd* self, const char* bind_host, int bind_port, 
                            uint64_t* bytes_in, uint64_t* bytes_out,
                            uint64_t* connections);
};

// Global class instance
extern const PeerxRinetdClassType PeerxRinetdClass;

#endif // PEERX_RINETD_INTERFACE_H 