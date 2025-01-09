#pragma once

/* Core Types */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;
typedef float f32;
typedef double f64;

/* Error Codes */
#define INFRA_OK                 0
#define INFRA_ERR_NOMEM         1
#define INFRA_ERR_INVALID       2
#define INFRA_ERR_NOTFOUND      3
#define INFRA_ERR_EXISTS        4
#define INFRA_ERR_BUSY          5
#define INFRA_ERR_TIMEOUT       6
#define INFRA_ERR_NETWORK       7
#define INFRA_ERR_CONN_REFUSED  8
#define INFRA_ERR_CONN_TIMEOUT  9
#define INFRA_ERR_CONN_CLOSED   10

/* Event Types */
#define INFRA_EVENT_READ    0x01
#define INFRA_EVENT_WRITE   0x02
#define INFRA_EVENT_ERROR   0x04

/* Utility Macros */
#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

#define infra_list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define infra_list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)

/* Linked List */
struct infra_list {
    struct infra_list* next;
    struct infra_list* prev;
};

void infra_list_init(struct infra_list* list);
void infra_list_add(struct infra_list* list, struct infra_list* node);
void infra_list_del(struct infra_list* node);
int infra_list_empty(struct infra_list* list);

/* Synchronization Primitives */
typedef struct infra_spinlock {
    volatile int locked;
} infra_spinlock_t;

struct infra_mutex {
    infra_spinlock_t lock;
    struct infra_list waiters;
};

typedef struct infra_mutex infra_mutex_t;

typedef struct infra_cond {
    struct infra_list waiters;
} infra_cond_t;

void infra_spin_init(infra_spinlock_t* lock);
void infra_spin_lock(infra_spinlock_t* lock);
void infra_spin_unlock(infra_spinlock_t* lock);
int infra_spin_trylock(infra_spinlock_t* lock);

void infra_mutex_init(infra_mutex_t* mutex);
void infra_mutex_destroy(infra_mutex_t* mutex);
void infra_mutex_lock(infra_mutex_t* mutex);
void infra_mutex_unlock(infra_mutex_t* mutex);
int infra_mutex_trylock(infra_mutex_t* mutex);

void infra_cond_init(infra_cond_t* cond);
void infra_cond_destroy(infra_cond_t* cond);
void infra_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex);
void infra_cond_signal(infra_cond_t* cond);
void infra_cond_broadcast(infra_cond_t* cond);

/* Event Loop Types */
typedef void (*infra_event_handler)(void* ctx, int events);

struct infra_event {
    struct infra_list list;
    int fd;
    int events;
    infra_event_handler handler;
    void* ctx;
};

struct infra_event_loop {
    void* impl;
};

/* Event Loop API */
int infra_event_loop_init(struct infra_event_loop* loop);
void infra_event_loop_destroy(struct infra_event_loop* loop);
int infra_event_loop_run(struct infra_event_loop* loop);
void infra_event_loop_stop(struct infra_event_loop* loop);

int infra_event_add(struct infra_event_loop* loop, struct infra_event* ev);
int infra_event_del(struct infra_event_loop* loop, struct infra_event* ev);
int infra_event_mod(struct infra_event_loop* loop, struct infra_event* ev);

/* IO Framework Types */
struct infra_io_context {
    struct infra_event_loop* loop;
    void* private_data;
};

typedef void (*infra_io_callback)(struct infra_io_context* ctx, int status);

struct infra_io_request {
    struct infra_event ev;
    infra_io_callback callback;
    void* buffer;
    size_t length;
    size_t offset;
};

/* IO Framework API */
int infra_io_context_init(struct infra_io_context* ctx, struct infra_event_loop* loop);
void infra_io_context_destroy(struct infra_io_context* ctx);

int infra_io_read(struct infra_io_context* ctx, int fd, void* buf, size_t len, infra_io_callback cb);
int infra_io_write(struct infra_io_context* ctx, int fd, const void* buf, size_t len, infra_io_callback cb);

/* Memory Management */
void* infra_malloc(size_t size);
void* infra_calloc(size_t nmemb, size_t size);
void* infra_realloc(void* ptr, size_t size);
void infra_free(void* ptr);

/* Error Handling */
const char* infra_strerror(int code);
void infra_set_error(int code, const char* msg);
const char* infra_get_error(void);

/* Hash Table */
struct infra_hash_node {
    struct infra_list list;
    u64 hash;
    void* key;
    void* value;
};

struct infra_hash_table {
    struct infra_list* buckets;
    size_t nbuckets;
    size_t size;
};

int infra_hash_init(struct infra_hash_table* table, size_t nbuckets);
void infra_hash_destroy(struct infra_hash_table* table);
int infra_hash_put(struct infra_hash_table* table, void* key, size_t klen, void* value);
void* infra_hash_get(struct infra_hash_table* table, void* key, size_t klen);
int infra_hash_del(struct infra_hash_table* table, void* key, size_t klen);

/* Async Framework */
typedef void (*infra_async_fn)(void* arg);

struct infra_async_task {
    struct infra_list list;
    infra_async_fn fn;
    void* arg;
    int status;
};

struct infra_async_queue {
    struct infra_list tasks;
    infra_mutex_t lock;
    infra_cond_t not_empty;
    size_t size;
    int shutdown;
};

int infra_async_queue_init(struct infra_async_queue* queue);
void infra_async_queue_destroy(struct infra_async_queue* queue);
int infra_async_queue_push(struct infra_async_queue* queue, infra_async_fn fn, void* arg);
int infra_async_queue_pop(struct infra_async_queue* queue, struct infra_async_task** task);
void infra_async_queue_shutdown(struct infra_async_queue* queue);

struct infra_async_worker {
    struct infra_list list;
    struct infra_async_queue* queue;
    void* thread;
    int running;
};

struct infra_async_pool {
    struct infra_async_queue queue;
    struct infra_list workers;
    size_t num_workers;
};

int infra_async_pool_init(struct infra_async_pool* pool, size_t num_workers);
void infra_async_pool_destroy(struct infra_async_pool* pool);
int infra_async_pool_submit(struct infra_async_pool* pool, infra_async_fn fn, void* arg);

/* Timer */
struct infra_timer {
    struct infra_list list;
    u64 deadline;
    infra_event_handler handler;
    void* ctx;
};

int infra_timer_init(struct infra_timer* timer, u64 deadline, infra_event_handler handler, void* ctx);
int infra_timer_add(struct infra_event_loop* loop, struct infra_timer* timer);
int infra_timer_del(struct infra_event_loop* loop, struct infra_timer* timer);

/* Buffer Management */
struct infra_buffer {
    void* data;
    size_t size;
    size_t capacity;
    size_t read_pos;
    size_t write_pos;
};

int infra_buffer_init(struct infra_buffer* buf, size_t initial_capacity);
void infra_buffer_destroy(struct infra_buffer* buf);
int infra_buffer_reserve(struct infra_buffer* buf, size_t size);
int infra_buffer_write(struct infra_buffer* buf, const void* data, size_t size);
int infra_buffer_read(struct infra_buffer* buf, void* data, size_t size);
size_t infra_buffer_readable(struct infra_buffer* buf);
size_t infra_buffer_writable(struct infra_buffer* buf);
void infra_buffer_reset(struct infra_buffer* buf);

/* IO Types */
typedef struct infra_io_ctx {
    struct infra_event event;
    void* buffer;
    size_t size;
    size_t offset;
    int (*callback)(struct infra_io_ctx* ctx, int status);
} infra_io_ctx_t;
