/*
 * infra_async.h - Unified Asynchronous System Interface
 */

#ifndef INFRA_ASYNC_H
#define INFRA_ASYNC_H

#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"

//-----------------------------------------------------------------------------
// Types and Constants
//-----------------------------------------------------------------------------

// Event types
#define INFRA_EVENT_NONE   0x00
#define INFRA_EVENT_READ   0x01
#define INFRA_EVENT_WRITE  0x02
#define INFRA_EVENT_ERROR  0x04
#define INFRA_EVENT_TIMER  0x08
#define INFRA_EVENT_SIGNAL 0x10

// Forward declarations
typedef struct infra_loop infra_loop_t;
typedef struct infra_event infra_event_t;
typedef struct infra_timer infra_timer_t;

// Event callback
typedef void (*infra_event_cb)(infra_event_t* event, void* arg);

// Timer callback
typedef void (*infra_timer_cb)(infra_timer_t* timer, void* arg);

// Event structure
struct infra_event {
    int fd;                  // File descriptor
    uint32_t events;         // Registered events
    infra_event_cb cb;       // Event callback
    void* arg;               // User argument
    infra_loop_t* loop;      // Owner loop
    void* data;              // Platform-specific data
};

// Timer structure
struct infra_timer {
    uint64_t interval;       // Timer interval in milliseconds
    uint64_t next_expire;    // Next expiration time
    bool repeat;             // Whether timer repeats
    infra_timer_cb cb;       // Timer callback
    void* arg;               // User argument
    infra_loop_t* loop;      // Owner loop
    void* data;              // Platform-specific data
};

//-----------------------------------------------------------------------------
// Loop Management
//-----------------------------------------------------------------------------

// Create event loop
infra_error_t infra_loop_create(infra_loop_t** loop);

// Destroy event loop
infra_error_t infra_loop_destroy(infra_loop_t* loop);

// Run event loop
infra_error_t infra_loop_run(infra_loop_t* loop);

// Stop event loop
infra_error_t infra_loop_stop(infra_loop_t* loop);

// Get loop statistics
infra_error_t infra_loop_stats(infra_loop_t* loop, infra_stats_t* stats);

//-----------------------------------------------------------------------------
// Event Management
//-----------------------------------------------------------------------------

// Create event
infra_error_t infra_event_create(infra_loop_t* loop, int fd, 
                                uint32_t events, infra_event_cb cb,
                                void* arg, infra_event_t** event);

// Destroy event
infra_error_t infra_event_destroy(infra_event_t* event);

// Modify event
infra_error_t infra_event_modify(infra_event_t* event, uint32_t events);

//-----------------------------------------------------------------------------
// Timer Management
//-----------------------------------------------------------------------------

// Create timer
infra_error_t infra_timer_create(infra_loop_t* loop, uint64_t interval,
                                bool repeat, infra_timer_cb cb,
                                void* arg, infra_timer_t** timer);

// Destroy timer
infra_error_t infra_timer_destroy(infra_timer_t* timer);

// Start timer
infra_error_t infra_timer_start(infra_timer_t* timer);

// Stop timer
infra_error_t infra_timer_stop(infra_timer_t* timer);

//-----------------------------------------------------------------------------
// Async IO Operations
//-----------------------------------------------------------------------------

// Async read
infra_error_t infra_async_read(infra_loop_t* loop, int fd,
                              void* buf, size_t len,
                              infra_event_cb cb, void* arg);

// Async write
infra_error_t infra_async_write(infra_loop_t* loop, int fd,
                               const void* buf, size_t len,
                               infra_event_cb cb, void* arg);

//-----------------------------------------------------------------------------
// Async Task Management
//-----------------------------------------------------------------------------

typedef struct infra_async_context infra_async_context_t;
typedef struct infra_async_task infra_async_task_t;
typedef void (*infra_async_callback_t)(infra_async_task_t* task, infra_error_t error);

typedef enum infra_async_type {
    INFRA_ASYNC_READ,
    INFRA_ASYNC_WRITE,
    INFRA_ASYNC_ACCEPT,
    INFRA_ASYNC_CONNECT,
    INFRA_ASYNC_TIMER,
    INFRA_ASYNC_EVENT
} infra_async_type_t;

struct infra_async_task {
    infra_async_type_t type;
    infra_async_callback_t callback;
    void* user_data;
    infra_stats_t* stats;
    union {
        struct {
            int fd;
            void* buffer;
            size_t size;
            size_t offset;
        } io;
        struct {
            int fd;
            struct sockaddr* addr;
            socklen_t* addrlen;
        } accept;
        struct {
            int fd;
            const struct sockaddr* addr;
            socklen_t addrlen;
        } connect;
        struct {
            uint64_t timeout_ms;
            bool periodic;
        } timer;
        struct {
            int event_fd;
            uint64_t value;
        } event;
    };
};

infra_error_t infra_async_init(infra_async_context_t** ctx);
void infra_async_destroy(infra_async_context_t* ctx);
infra_error_t infra_async_submit(infra_async_context_t* ctx,
                                infra_async_task_t* task);
infra_error_t infra_async_cancel(infra_async_context_t* ctx,
                                infra_async_task_t* task);
infra_error_t infra_async_run(infra_async_context_t* ctx, uint64_t timeout_ms);
infra_error_t infra_async_stop(infra_async_context_t* ctx);

#endif /* INFRA_ASYNC_H */ 