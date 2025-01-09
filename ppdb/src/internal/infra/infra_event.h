#pragma once

#include "internal/infra/infra.h"

/* Event Loop Functions */
int event_loop_init(void);
void event_loop_cleanup(void);
int event_loop_run(void);
void event_loop_stop(void);

/* Event Handling */
typedef void (*event_handler_fn)(int fd, void* arg);

struct event_handler {
    event_handler_fn fn;
    void* arg;
};

int event_add_handler(struct infra_event_loop* loop, int fd, event_handler_fn handler, void* arg);
int event_del_handler(struct infra_event_loop* loop, int fd);
int event_add_io(struct infra_event_loop* loop, int fd, int events, void (*handler)(int fd, void* arg), void* arg);
int event_mod_io(struct infra_event_loop* loop, int fd, int events); 