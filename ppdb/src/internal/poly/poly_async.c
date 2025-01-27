//TODO change select to poll mechanism
#include "poly_async.h"

#define POLY_ASYNC_MAX_EVENTS 1024

struct poly_async_future {
    int fd;                         /* File descriptor */
    int events;                     /* Monitored events */
    poly_async_callback_t callback; /* User callback */
    void* user_data;               /* User context */
    struct poly_async_context* ctx; /* Parent context */
    bool cancelled;                /* Cancellation flag */
};

struct poly_async_context {
    bool running;                  /* Event loop running flag */
    int max_fd;                    /* Max fd for select */
    fd_set read_fds;              /* Read fd set */
    fd_set write_fds;             /* Write fd set */
    fd_set error_fds;             /* Error fd set */
    poly_async_future_t* futures[POLY_ASYNC_MAX_EVENTS]; /* Active futures */
};

poly_async_context_t* poly_async_create(void) {
    poly_async_context_t* ctx = calloc(1, sizeof(poly_async_context_t));
    if (!ctx) return NULL;
    
    FD_ZERO(&ctx->read_fds);
    FD_ZERO(&ctx->write_fds);
    FD_ZERO(&ctx->error_fds);
    ctx->max_fd = -1;
    
    return ctx;
}

void poly_async_destroy(poly_async_context_t* ctx) {
    if (!ctx) return;
    
    /* Cancel all pending futures */
    for (int i = 0; i < POLY_ASYNC_MAX_EVENTS; i++) {
        if (ctx->futures[i]) {
            poly_async_cancel(ctx->futures[i]);
            free(ctx->futures[i]);
        }
    }
    
    free(ctx);
}

poly_async_future_t* poly_async_add_fd(poly_async_context_t* ctx,
                                      int fd,
                                      int events,
                                      poly_async_callback_t callback,
                                      void* user_data) {
    if (!ctx || fd < 0 || !callback) return NULL;
    
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < POLY_ASYNC_MAX_EVENTS; i++) {
        if (!ctx->futures[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return NULL;
    
    /* Create future */
    poly_async_future_t* future = calloc(1, sizeof(poly_async_future_t));
    if (!future) return NULL;
    
    future->fd = fd;
    future->events = events;
    future->callback = callback;
    future->user_data = user_data;
    future->ctx = ctx;
    
    /* Update fd sets */
    if (events & POLY_ASYNC_READ) FD_SET(fd, &ctx->read_fds);
    if (events & POLY_ASYNC_WRITE) FD_SET(fd, &ctx->write_fds);
    if (events & POLY_ASYNC_ERROR) FD_SET(fd, &ctx->error_fds);
    
    if (fd > ctx->max_fd) ctx->max_fd = fd;
    
    ctx->futures[slot] = future;
    return future;
}

int poly_async_remove_fd(poly_async_context_t* ctx, int fd) {
    if (!ctx || fd < 0) return -1;
    
    /* Find and remove future */
    for (int i = 0; i < POLY_ASYNC_MAX_EVENTS; i++) {
        poly_async_future_t* future = ctx->futures[i];
        if (future && future->fd == fd) {
            FD_CLR(fd, &ctx->read_fds);
            FD_CLR(fd, &ctx->write_fds);
            FD_CLR(fd, &ctx->error_fds);
            
            free(future);
            ctx->futures[i] = NULL;
            
            /* Update max_fd if needed */
            if (fd == ctx->max_fd) {
                int new_max = -1;
                for (int j = 0; j < POLY_ASYNC_MAX_EVENTS; j++) {
                    if (ctx->futures[j] && ctx->futures[j]->fd > new_max) {
                        new_max = ctx->futures[j]->fd;
                    }
                }
                ctx->max_fd = new_max;
            }
            
            return 0;
        }
    }
    
    return -1;
}

int poly_async_run(poly_async_context_t* ctx) {
    if (!ctx) return -1;
    
    ctx->running = true;
    while (ctx->running) {
        fd_set read_fds = ctx->read_fds;
        fd_set write_fds = ctx->write_fds;
        fd_set error_fds = ctx->error_fds;
        
        int ready = select(ctx->max_fd + 1, &read_fds, &write_fds, &error_fds, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        
        /* Process ready descriptors */
        for (int i = 0; i < POLY_ASYNC_MAX_EVENTS && ready > 0; i++) {
            poly_async_future_t* future = ctx->futures[i];
            if (!future || future->cancelled) continue;
            
            int events = 0;
            size_t bytes = 0;
            
            if (FD_ISSET(future->fd, &read_fds)) events |= POLY_ASYNC_READ;
            if (FD_ISSET(future->fd, &write_fds)) events |= POLY_ASYNC_WRITE;
            if (FD_ISSET(future->fd, &error_fds)) events |= POLY_ASYNC_ERROR;
            
            if (events) {
                future->callback(future->user_data, 0, bytes);
                ready--;
            }
        }
    }
    
    return 0;
}

void poly_async_stop(poly_async_context_t* ctx) {
    if (ctx) ctx->running = false;
}

void poly_async_cancel(poly_async_future_t* future) {
    if (future) future->cancelled = true;
} 