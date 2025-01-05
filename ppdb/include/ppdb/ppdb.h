#ifndef PPDB_H
#define PPDB_H

#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// Error Handling
//-----------------------------------------------------------------------------
typedef int32_t ppdb_error_t;

#define PPDB_OK                    0
#define PPDB_ERR_NULL_POINTER      1
#define PPDB_ERR_INVALID_ARGUMENT  2
#define PPDB_ERR_INVALID_SIZE      3
#define PPDB_ERR_INVALID_STATE     4
#define PPDB_ERR_OUT_OF_MEMORY     5
#define PPDB_ERR_FULL             6
#define PPDB_ERR_NOT_IMPLEMENTED   7
#define PPDB_ERR_BUSY             8
#define PPDB_ERR_TIMEOUT          9

//-----------------------------------------------------------------------------
// Public Types
//-----------------------------------------------------------------------------
// Opaque handles
typedef uint64_t ppdb_context_t;
typedef uint64_t ppdb_cursor_t;
typedef uint64_t ppdb_batch_t;
typedef uint64_t ppdb_sync_t;
typedef uint64_t ppdb_storage_t;
typedef uint64_t ppdb_peer_t;

// Data buffer
typedef struct ppdb_data {
    uint8_t inline_data[32];  // Small data optimization
    uint32_t size;
    uint32_t flags;
    void* extended_data;  // For data larger than inline buffer
} ppdb_data_t;

// Synchronization
typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX = 1,
    PPDB_SYNC_RWLOCK = 2,
} ppdb_sync_type_t;

typedef struct ppdb_sync_config {
    ppdb_sync_type_t type;
    bool use_lockfree;
    uint32_t max_readers;
    uint32_t backoff_us;
    uint32_t max_retries;
} ppdb_sync_config_t;

// Storage
typedef struct ppdb_storage_config {
    const char* path;
    uint32_t block_size;
    uint32_t cache_size;
    bool sync_writes;
} ppdb_storage_config_t;

// Peer
typedef struct ppdb_peer_config {
    const char* host;
    uint16_t port;
    uint32_t timeout_ms;
} ppdb_peer_config_t;

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------
// Context Management
ppdb_error_t ppdb_context_create(ppdb_context_t* ctx);
void ppdb_context_destroy(ppdb_context_t ctx);
ppdb_error_t ppdb_context_get_state(ppdb_context_t ctx, uint32_t* state);

// Data Management
ppdb_error_t ppdb_data_create(const void* data, size_t size, ppdb_data_t* out);
ppdb_error_t ppdb_data_destroy(ppdb_data_t* data);
ppdb_error_t ppdb_data_get(const ppdb_data_t* data, void* buf, size_t size, size_t* copied);
ppdb_error_t ppdb_data_size(const ppdb_data_t* data, size_t* size);

// Cursor Operations
ppdb_error_t ppdb_cursor_create(ppdb_context_t ctx, ppdb_cursor_t* cursor);
void ppdb_cursor_destroy(ppdb_cursor_t cursor);
ppdb_error_t ppdb_cursor_next(ppdb_cursor_t cursor, ppdb_data_t* key, ppdb_data_t* value);
ppdb_error_t ppdb_cursor_prev(ppdb_cursor_t cursor, ppdb_data_t* key, ppdb_data_t* value);
ppdb_error_t ppdb_cursor_seek(ppdb_cursor_t cursor, const ppdb_data_t* key);

// Batch Operations
ppdb_error_t ppdb_batch_create(ppdb_context_t ctx, ppdb_batch_t* batch);
void ppdb_batch_destroy(ppdb_batch_t batch);
ppdb_error_t ppdb_batch_put(ppdb_batch_t batch, const ppdb_data_t* key, const ppdb_data_t* value);
ppdb_error_t ppdb_batch_delete(ppdb_batch_t batch, const ppdb_data_t* key);
ppdb_error_t ppdb_batch_commit(ppdb_batch_t batch);
ppdb_error_t ppdb_batch_clear(ppdb_batch_t batch);

// Synchronization
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, const ppdb_sync_config_t* config);
void ppdb_sync_destroy(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync);

// Storage
ppdb_error_t ppdb_storage_create(ppdb_storage_t** storage, const ppdb_storage_config_t* config);
void ppdb_storage_destroy(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_read(ppdb_storage_t* storage, uint64_t offset, void* buf, size_t size);
ppdb_error_t ppdb_storage_write(ppdb_storage_t* storage, uint64_t offset, const void* buf, size_t size);
ppdb_error_t ppdb_storage_sync(ppdb_storage_t* storage);

// Peer Communication
ppdb_error_t ppdb_peer_create(ppdb_peer_t** peer, const ppdb_peer_config_t* config);
void ppdb_peer_destroy(ppdb_peer_t* peer);
ppdb_error_t ppdb_peer_connect(ppdb_peer_t* peer);
ppdb_error_t ppdb_peer_disconnect(ppdb_peer_t* peer);
ppdb_error_t ppdb_peer_send(ppdb_peer_t* peer, const void* buf, size_t size);
ppdb_error_t ppdb_peer_recv(ppdb_peer_t* peer, void* buf, size_t size, size_t* received);

#endif // PPDB_H
