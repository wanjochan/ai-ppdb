#ifndef PEER_MEMKV_H
#define PEER_MEMKV_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_platform.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/poly/poly_atomic.h"
#include "internal/peer/peer_service.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION          "1.0.0"
#define MEMKV_BUFFER_SIZE      8192
#define MEMKV_MAX_KEY_SIZE     250
#define MEMKV_MAX_VALUE_SIZE   (1024 * 1024)  // 1MB
#define MEMKV_MAX_CONNECTIONS  10000
#define MEMKV_DEFAULT_PORT     11211

// 线程池配置
#define MEMKV_MIN_THREADS      4
#define MEMKV_MAX_THREADS      32
#define MEMKV_QUEUE_SIZE       1000
#define MEMKV_IDLE_TIMEOUT     60

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// Item structure
typedef struct memkv_item {
    char* key;                  // Key
    void* value;                // Value
    size_t value_size;          // Value size
    uint32_t flags;             // Flags
    uint32_t exptime;           // Expiration time
    uint64_t cas;               // CAS value
    struct memkv_item* next;    // Next item in chain
} memkv_item_t;

// Statistics structure
typedef struct memkv_stats {
    poly_atomic_t cmd_get;      // Get commands
    poly_atomic_t cmd_set;      // Set commands
    poly_atomic_t cmd_delete;   // Delete commands
    poly_atomic_t hits;         // Cache hits
    poly_atomic_t misses;       // Cache misses
    poly_atomic_t curr_items;   // Current items
    poly_atomic_t total_items;  // Total items
    poly_atomic_t bytes;        // Current bytes used
} memkv_stats_t;

// Context structure
typedef struct memkv_context {
    bool is_running;                // Service running flag
    uint16_t port;                  // Listening port
    infra_socket_t sock;           // Listening socket
    infra_mutex_t mutex;           // Global mutex
    poly_hashtable_t* store;       // Key-value store
    poly_atomic_t cas_counter;     // CAS counter
    memkv_stats_t stats;           // Statistics
} memkv_context_t;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// Global context
extern memkv_context_t g_memkv_context;

// Service instance
extern peer_service_t g_memkv_service;

#endif // PEER_MEMKV_H
