/*
 * infra_async.h - Asynchronous System Interface
 */

#ifndef PPDB_INFRA_ASYNC_H
#define PPDB_INFRA_ASYNC_H

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Error Codes
//-----------------------------------------------------------------------------

#define PPDB_ERR_IO      10
#define PPDB_ERR_TIMEOUT 11
#define PPDB_ERR_CANCEL  12

//-----------------------------------------------------------------------------
// Type Definitions
//-----------------------------------------------------------------------------

typedef void (*ppdb_async_func_t)(void*);
typedef void (*ppdb_async_callback_t)(ppdb_error_t error, void* arg);

typedef enum {
    PPDB_ASYNC_STATE_INIT,
    PPDB_ASYNC_STATE_QUEUED,
    PPDB_ASYNC_STATE_RUNNING,
    PPDB_ASYNC_STATE_COMPLETED,
    PPDB_ASYNC_STATE_CANCELLED
} ppdb_async_state_t;

typedef struct ppdb_async_handle {
    struct ppdb_async_loop* loop;
    ppdb_async_state_t state;
    ppdb_async_func_t func;
    void* func_arg;
    ppdb_async_callback_t callback;
    void* callback_arg;
    struct ppdb_async_handle* next;
    uint64_t submit_time;
    uint64_t start_time;
    uint64_t complete_time;
} ppdb_async_handle_t;

typedef struct ppdb_async_io_stats {
    uint64_t total_requests;
    uint64_t completed_requests;
    uint64_t failed_requests;
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t total_wait_time;
    uint64_t total_exec_time;
} ppdb_async_io_stats_t;

typedef struct ppdb_async_queue {
    ppdb_async_handle_t* head;
    ppdb_async_handle_t* tail;
    size_t size;
} ppdb_async_queue_t;

typedef struct ppdb_async_loop {
    bool running;
    ppdb_mutex_t* lock;
    ppdb_cond_t* cond;
    ppdb_async_queue_t queues[3];  // ready, running, completed
    ppdb_async_io_stats_t io_stats;
} ppdb_async_loop_t;

//-----------------------------------------------------------------------------
// Function Declarations
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_async_loop_create(ppdb_async_loop_t** loop);
ppdb_error_t ppdb_async_loop_destroy(ppdb_async_loop_t* loop);
ppdb_error_t ppdb_async_loop_run(ppdb_async_loop_t* loop, uint32_t timeout_ms);
ppdb_error_t ppdb_async_loop_stop(ppdb_async_loop_t* loop);

ppdb_error_t ppdb_async_submit(ppdb_async_loop_t* loop,
                              ppdb_async_func_t func,
                              void* func_arg,
                              uint32_t flags,
                              uint32_t timeout_ms,
                              ppdb_async_callback_t callback,
                              void* callback_arg,
                              ppdb_async_handle_t** handle);

ppdb_error_t ppdb_async_cancel(ppdb_async_handle_t* handle);

ppdb_error_t ppdb_async_read(ppdb_async_loop_t* loop,
                            int fd,
                            void* buf,
                            size_t count,
                            uint64_t offset,
                            ppdb_async_callback_t callback,
                            void* user_data);

ppdb_error_t ppdb_async_write(ppdb_async_loop_t* loop,
                             int fd,
                             const void* buf,
                             size_t count,
                             uint64_t offset,
                             ppdb_async_callback_t callback,
                             void* user_data);

ppdb_error_t ppdb_async_fsync(ppdb_async_loop_t* loop,
                             int fd,
                             ppdb_async_callback_t callback,
                             void* user_data);

void ppdb_async_get_io_stats(ppdb_async_loop_t* loop,
                            ppdb_async_io_stats_t* stats);

#endif /* PPDB_INFRA_ASYNC_H */ 