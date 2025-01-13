#ifndef PEER_RINETD_H
#define PEER_RINETD_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_mux.h"
#include "internal/poly/poly_cmdline.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Maximum number of forwarding rules
#define RINETD_MAX_RULES 32
// Maximum buffer size for data forwarding
#define RINETD_BUFFER_SIZE 8192
// Maximum address length
#define RINETD_MAX_ADDR_LEN 64
// Maximum path length
#define RINETD_MAX_PATH_LEN 256

//-----------------------------------------------------------------------------
// Data Structures
//-----------------------------------------------------------------------------

// Forwarding rule structure
typedef struct {
    char src_addr[RINETD_MAX_ADDR_LEN];  // Source address
    int src_port;                        // Source port
    char dst_addr[RINETD_MAX_ADDR_LEN];  // Destination address
    int dst_port;                        // Destination port
    bool enabled;                        // Whether this rule is enabled
    // TODO: Add statistics fields (bytes forwarded, connections handled, etc.)
} rinetd_rule_t;

// Forward session structure
typedef struct {
    infra_socket_t client_sock;         // Client socket
    infra_socket_t server_sock;         // Server socket
    rinetd_rule_t* rule;                // Associated rule
    infra_thread_t forward_thread;      // Client to server forwarding thread
    infra_thread_t backward_thread;     // Server to client forwarding thread
    bool active;                        // Whether session is active
    // TODO: Add session statistics
} rinetd_session_t;

// Service context structure
typedef struct {
    bool running;                        // Whether service is running
    char config_path[RINETD_MAX_PATH_LEN]; // Config file path
    rinetd_rule_t rules[RINETD_MAX_RULES]; // Forwarding rules
    int rule_count;                      // Number of rules
    infra_mux_t* mux;                   // Event multiplexer
    infra_mutex_t mutex;                // Thread synchronization
    infra_thread_t listener_threads[RINETD_MAX_RULES]; // Listener threads
    rinetd_session_t active_sessions[RINETD_MAX_RULES]; // Active forwarding sessions
    int session_count;                   // Number of active sessions
    // TODO: Add service statistics
} rinetd_context_t;

//-----------------------------------------------------------------------------
// Function Declarations
//-----------------------------------------------------------------------------

// Initialize the rinetd service
infra_error_t rinetd_init(void);

// Cleanup the rinetd service
infra_error_t rinetd_cleanup(void);

// Load configuration from file
infra_error_t rinetd_load_config(const char* path);

// Save configuration to file
infra_error_t rinetd_save_config(const char* path);

// Add a forwarding rule
infra_error_t rinetd_add_rule(const rinetd_rule_t* rule);

// Remove a forwarding rule by index
infra_error_t rinetd_remove_rule(int index);

// Get a forwarding rule by index
infra_error_t rinetd_get_rule(int index, rinetd_rule_t* rule);

// Enable/disable a forwarding rule
infra_error_t rinetd_enable_rule(int index, bool enable);

// Check if service is running
bool rinetd_is_running(void);

// Command line options and handler
extern const poly_cmd_option_t rinetd_options[];
extern const int rinetd_option_count;
infra_error_t rinetd_cmd_handler(int argc, char** argv);

#endif // PEER_RINETD_H 