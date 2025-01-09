#pragma once

#include "internal/infra/infra.h"

/* IO Event Types */
#define EVENT_READ  0x01
#define EVENT_WRITE 0x02
#define EVENT_ERROR 0x04

/* IO Callback Type */
typedef void (*io_callback_fn)(int status, void* user_data);

/* IO Functions */
int io_init(void);
void io_cleanup(void);

int io_read_async(struct infra_event_loop* loop, int fd, void* buf, size_t len, io_callback_fn callback, void* user_data);
int io_write_async(struct infra_event_loop* loop, int fd, const void* buf, size_t len, io_callback_fn callback, void* user_data);

/* Event Loop Integration */
int event_add_io(struct infra_event_loop* loop, int fd, int events, void (*handler)(int fd, void* arg), void* arg); 