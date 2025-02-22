#ifndef PEERX_RINETD_H
#define PEERX_RINETD_H

#include "internal/polyx/PolyxService.h"

// Forward declarations
typedef struct PeerxRinetd PeerxRinetd;
typedef struct PeerxRinetdClassType PeerxRinetdClassType;

// Rule configuration
typedef struct {
    char bind_host[64];
    InfraxU16 bind_port;
    char target_host[64];
    InfraxU16 target_port;
    InfraxBool enabled;
} peerx_rinetd_rule_t;

// Rinetd service instance
struct PeerxRinetd {
    // Base service
    PolyxService service;
};

// Rinetd service class interface
struct PeerxRinetdClassType {
    // Service factory
    PolyxService* (*create_service)(void);
    
    // Rule management
    InfraxError (*add_rule)(PeerxRinetd* self, const peerx_rinetd_rule_t* rule);
    InfraxError (*remove_rule)(PeerxRinetd* self, const char* bind_host, InfraxU16 bind_port);
    InfraxError (*enable_rule)(PeerxRinetd* self, const char* bind_host, InfraxU16 bind_port);
    InfraxError (*disable_rule)(PeerxRinetd* self, const char* bind_host, InfraxU16 bind_port);
    InfraxError (*get_rules)(PeerxRinetd* self, peerx_rinetd_rule_t* rules, InfraxSize* count);
    
    // Statistics
    InfraxError (*get_stats)(PeerxRinetd* self, const char* bind_host, InfraxU16 bind_port,
                            InfraxU64* bytes_in, InfraxU64* bytes_out,
                            InfraxU64* connections);
};

// Global class instance
extern const PeerxRinetdClassType PeerxRinetdClass;

#endif // PEERX_RINETD_H 