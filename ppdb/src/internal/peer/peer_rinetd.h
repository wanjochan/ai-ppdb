#ifndef PEER_RINETD_H
#define PEER_RINETD_H

#include "internal/infra/infra_core.h"
#include "internal/poly/poly_cmdline.h"

//-----------------------------------------------------------------------------
// Data Structures
//-----------------------------------------------------------------------------

// Maximum number of forwarding rules
#define RINETD_MAX_RULES 32
// Maximum buffer size for data forwarding
#define RINETD_BUFFER_SIZE 8192

// Forwarding rule structure
typedef struct {
    char src_addr[INFRA_MAX_ADDR_LEN];  // Source address
    int src_port;                        // Source port
    char dst_addr[INFRA_MAX_ADDR_LEN];  // Destination address
    int dst_port;                        // Destination port
    bool enabled;                        // Whether this rule is enabled
    // TODO: Add statistics fields (bytes forwarded, connections handled, etc.)
} rinetd_rule_t;

// Forward session structure
typedef struct {
    infra_socket_t client_sock;         // Client socket
    infra_socket_t server_sock;         // Server socket
    rinetd_rule_t* rule;                // Associated rule
    infra_thread_t* thread;             // Forwarding thread
    bool active;                        // Whether session is active
    // TODO: Add session statistics
} rinetd_session_t;

// Service context structure
typedef struct {
    bool running;                        // Whether service is running
    char config_path[INFRA_MAX_PATH_LEN]; // Config file path
    rinetd_rule_t rules[RINETD_MAX_RULES]; // Forwarding rules
    int rule_count;                      // Number of rules
    infra_mux_t* mux;                   // Event multiplexer
    infra_mutex_t* mutex;               // Thread synchronization
    infra_thread_t* listener_threads[RINETD_MAX_RULES]; // Listener threads
    rinetd_session_t* active_sessions;   // Active forwarding sessions
    int session_count;                   // Number of active sessions
    // TODO: Add service statistics
} rinetd_context_t;

//-----------------------------------------------------------------------------
// Core Functions
//-----------------------------------------------------------------------------

// Initialize rinetd service
infra_error_t rinetd_init(void);

// Cleanup rinetd service
infra_error_t rinetd_cleanup(void);

// Load configuration from file
infra_error_t rinetd_load_config(const char* path);

// Save configuration to file
infra_error_t rinetd_save_config(const char* path);

// Add a forwarding rule
infra_error_t rinetd_add_rule(const rinetd_rule_t* rule);

// Remove a forwarding rule
infra_error_t rinetd_remove_rule(int index);

// Get service status
bool rinetd_is_running(void);

// TODO: Add functions for statistics collection and reporting
// TODO: Add functions for configuration hot reload

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

extern const poly_cmd_option_t rinetd_options[];
extern const int rinetd_option_count;

//-----------------------------------------------------------------------------
// Command Handler
//-----------------------------------------------------------------------------

infra_error_t rinetd_cmd_handler(int argc, char** argv);

#endif // PEER_RINETD_H 