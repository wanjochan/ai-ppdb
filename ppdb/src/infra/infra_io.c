#include "cosmopolitan.h"
#include "internal/infra/infra.h"

// Print operations
int infra_printf(const char* format, ...) {
    if (!format) return -1;
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

int infra_dprintf(int fd, const char* format, ...) {
    if (fd < 0 || !format) return -1;
    va_list args;
    va_start(args, format);
    int result = vdprintf(fd, format, args);
    va_end(args);
    return result;
}

int infra_puts(const char* str) {
    if (!str) return -1;
    return puts(str);
}

int infra_putchar(int ch) {
    return putchar(ch);
}

// Read from file descriptor
int infra_io_read(int fd, void* buf, size_t count) {
    if (fd < 0 || !buf || count == 0) return -1;
    
    ssize_t n = read(fd, buf, count);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    
    return (int)n;
}

// Write to file descriptor
int infra_io_write(int fd, const void* buf, size_t count) {
    if (fd < 0 || !buf || count == 0) return -1;
    
    ssize_t n = write(fd, buf, count);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    
    return (int)n;
}

// IO context for async operations
typedef struct infra_io_context {
    infra_event_t event;
    void* buffer;
    size_t count;
    size_t offset;
    infra_event_handler handler;
} infra_io_context_t;

// Handle read events
static void handle_read(infra_event_t* event, uint32_t events) {
    infra_io_context_t* ctx = (infra_io_context_t*)event;
    
    if (events & INFRA_EVENT_ERROR) {
        ctx->handler(event, INFRA_EVENT_ERROR);
        free(ctx);
        return;
    }
    
    if (events & INFRA_EVENT_READ) {
        ssize_t n = read(event->fd, (char*)ctx->buffer + ctx->offset, ctx->count - ctx->offset);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ctx->handler(event, INFRA_EVENT_ERROR);
                free(ctx);
                return;
            }
        } else if (n == 0) {
            ctx->handler(event, INFRA_EVENT_ERROR);
            free(ctx);
            return;
        } else {
            ctx->offset += n;
            if (ctx->offset == ctx->count) {
                ctx->handler(event, INFRA_EVENT_READ);
                free(ctx);
                return;
            }
        }
    }
}

// Handle write events
static void handle_write(infra_event_t* event, uint32_t events) {
    infra_io_context_t* ctx = (infra_io_context_t*)event;
    
    if (events & INFRA_EVENT_ERROR) {
        ctx->handler(event, INFRA_EVENT_ERROR);
        free(ctx);
        return;
    }
    
    if (events & INFRA_EVENT_WRITE) {
        ssize_t n = write(event->fd, (char*)ctx->buffer + ctx->offset, ctx->count - ctx->offset);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ctx->handler(event, INFRA_EVENT_ERROR);
                free(ctx);
                return;
            }
        } else {
            ctx->offset += n;
            if (ctx->offset == ctx->count) {
                ctx->handler(event, INFRA_EVENT_WRITE);
                free(ctx);
                return;
            }
        }
    }
}

// Async read
int infra_io_read_async(infra_event_loop_t* loop, int fd, void* buf, size_t count, infra_event_handler handler) {
    if (!loop || fd < 0 || !buf || count == 0 || !handler) return -1;
    
    infra_io_context_t* ctx = malloc(sizeof(infra_io_context_t));
    if (!ctx) return -1;
    
    ctx->event.fd = fd;
    ctx->event.events = INFRA_EVENT_READ;
    ctx->event.handler = handle_read;
    ctx->event.user_data = NULL;
    ctx->buffer = buf;
    ctx->count = count;
    ctx->offset = 0;
    ctx->handler = handler;
    
    if (infra_event_add(loop, &ctx->event) < 0) {
        free(ctx);
        return -1;
    }
    
    return 0;
}

// Async write
int infra_io_write_async(infra_event_loop_t* loop, int fd, const void* buf, size_t count, infra_event_handler handler) {
    if (!loop || fd < 0 || !buf || count == 0 || !handler) return -1;
    
    infra_io_context_t* ctx = malloc(sizeof(infra_io_context_t));
    if (!ctx) return -1;
    
    ctx->event.fd = fd;
    ctx->event.events = INFRA_EVENT_WRITE;
    ctx->event.handler = handle_write;
    ctx->event.user_data = NULL;
    ctx->buffer = (void*)buf;
    ctx->count = count;
    ctx->offset = 0;
    ctx->handler = handler;
    
    if (infra_event_add(loop, &ctx->event) < 0) {
        free(ctx);
        return -1;
    }
    
    return 0;
}
