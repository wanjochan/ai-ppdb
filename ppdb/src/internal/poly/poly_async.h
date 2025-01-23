#ifndef POLY_ASYNC_H_
#define POLY_ASYNC_H_

#include "libc/integral/integral.h"
#include "libc/runtime/runtime.h"
#include "libc/sysv/consts/poll.h"
#include "poly_error.h"

/**
 * @brief Event types for async operations
 */
typedef enum {
    POLY_ASYNC_READ = 1,   /* Read event */
    POLY_ASYNC_WRITE = 2,  /* Write event */
    POLY_ASYNC_ERROR = 4   /* Error event */
} poly_async_event_t;

/**
 * @brief Async context handle
 */
typedef struct poly_async_context poly_async_context_t;

/**
 * @brief Async future handle for operation results
 */
typedef struct poly_async_future poly_async_future_t;

/**
 * @brief Callback function type for async operations
 * @param user_data User provided context
 * @param status Operation status (0 for success)
 * @param bytes_transferred Number of bytes transferred (if applicable)
 */
typedef void (*poly_async_callback_t)(void* user_data, int status, size_t bytes_transferred);

/**
 * @brief Create an async context
 * @return Context handle or NULL on error
 */
poly_async_context_t* poly_async_create(void);

/**
 * @brief Destroy an async context
 * @param ctx Context to destroy
 */
void poly_async_destroy(poly_async_context_t* ctx);

/**
 * @brief Add a file descriptor to async context
 * @param ctx Async context
 * @param fd File descriptor to monitor
 * @param events Event types to monitor (POLY_ASYNC_*)
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return Future handle or NULL on error
 */
poly_async_future_t* poly_async_add_fd(poly_async_context_t* ctx, 
                                      int fd, 
                                      int events,
                                      poly_async_callback_t callback,
                                      void* user_data);

/**
 * @brief Remove a file descriptor from async context
 * @param ctx Async context
 * @param fd File descriptor to remove
 * @return 0 on success, error code otherwise
 */
int poly_async_remove_fd(poly_async_context_t* ctx, int fd);

/**
 * @brief Run the async event loop
 * @param ctx Async context
 * @return 0 on success, error code otherwise
 */
int poly_async_run(poly_async_context_t* ctx);

/**
 * @brief Stop the async event loop
 * @param ctx Async context
 */
void poly_async_stop(poly_async_context_t* ctx);

/**
 * @brief Cancel a future
 * @param future Future to cancel
 */
void poly_async_cancel(poly_async_future_t* future);

#endif /* POLY_ASYNC_H_ */ 