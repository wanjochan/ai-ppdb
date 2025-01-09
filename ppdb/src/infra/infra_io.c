#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_io.h"
#include "internal/infra/infra_event.h"

// Forward declarations of internal functions
static void handle_read(int fd, void *data);
static void handle_write(int fd, void *data);

// IO context structure
typedef struct {
    int fd;
    void *buf;
    size_t len;
    size_t offset;
    io_callback_fn callback;
    void *user_data;
    struct infra_event_loop* loop;
} io_context_t;

// Initialize IO subsystem
int io_init(void) {
    return 0;
}

// Async read operation
int io_read_async(struct infra_event_loop* loop, int fd, void *buf, size_t len, io_callback_fn callback, void *user_data) {
    io_context_t *ctx = infra_malloc(sizeof(io_context_t));
    if (!ctx) {
        infra_set_error(INFRA_ERR_NOMEM, "Failed to allocate IO context");
        return -1;
    }
    
    ctx->fd = fd;
    ctx->buf = buf;
    ctx->len = len;
    ctx->offset = 0;
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->loop = loop;
    
    // Register with event loop for read events
    if (event_add_io(loop, fd, EVENT_READ, handle_read, ctx) != 0) {
        infra_free(ctx);
        return -1;
    }
    
    return 0;
}

// Async write operation
int io_write_async(struct infra_event_loop* loop, int fd, const void *buf, size_t len, io_callback_fn callback, void *user_data) {
    io_context_t *ctx = infra_malloc(sizeof(io_context_t));
    if (!ctx) {
        infra_set_error(INFRA_ERR_NOMEM, "Failed to allocate IO context");
        return -1;
    }
    
    ctx->fd = fd;
    ctx->buf = (void*)buf;
    ctx->len = len;
    ctx->offset = 0;
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->loop = loop;
    
    // Register with event loop for write events
    if (event_add_io(loop, fd, EVENT_WRITE, handle_write, ctx) != 0) {
        infra_free(ctx);
        return -1;
    }
    
    return 0;
}

// Internal read handler
static void handle_read(int fd, void *data) {
    io_context_t *ctx = (io_context_t*)data;
    ssize_t n;
    
    n = read(fd, ctx->buf + ctx->offset, ctx->len - ctx->offset);
    
    if (n > 0) {
        ctx->offset += n;
        if (ctx->offset == ctx->len) {
            // Read complete
            event_del_handler(ctx->loop, fd);
            ctx->callback(0, ctx->user_data);
            infra_free(ctx);
        }
    } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
        // Error or EOF
        event_del_handler(ctx->loop, fd);
        ctx->callback(-1, ctx->user_data);
        infra_free(ctx);
    }
}

// Internal write handler
static void handle_write(int fd, void *data) {
    io_context_t *ctx = (io_context_t*)data;
    ssize_t n;
    
    n = write(fd, ctx->buf + ctx->offset, ctx->len - ctx->offset);
    
    if (n > 0) {
        ctx->offset += n;
        if (ctx->offset == ctx->len) {
            // Write complete
            event_del_handler(ctx->loop, fd);
            ctx->callback(0, ctx->user_data);
            infra_free(ctx);
        }
    } else if (n < 0 && errno != EAGAIN) {
        // Error
        event_del_handler(ctx->loop, fd);
        ctx->callback(-1, ctx->user_data);
        infra_free(ctx);
    }
}

// Cleanup IO subsystem
void io_cleanup(void) {
    // Nothing to cleanup
}
