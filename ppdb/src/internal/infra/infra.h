#ifndef PPDB_INFRA_H
#define PPDB_INFRA_H

#include "cosmopolitan.h"

/* Basic Types */
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;

/* Error Codes */
#define INFRA_OK           0
#define INFRA_ERR_NOMEM    1
#define INFRA_ERR_INVALID  2
#define INFRA_ERR_NOTFOUND 3
#define INFRA_ERR_EXISTS   4

/* Log Levels */
#define INFRA_LOG_ERROR 0
#define INFRA_LOG_WARN  1
#define INFRA_LOG_INFO  2
#define INFRA_LOG_DEBUG 3

/* Statistics Structure */
struct infra_stats {
    u64 alloc_count;
    u64 free_count;
    u64 total_allocated;
    u64 current_allocated;
    u64 error_count;
};

/* Core Functions */
void infra_set_log_level(int level);
void infra_set_log_handler(void (*handler)(int level, const char* msg));
void infra_log(int level, const char* fmt, ...);
const char* infra_strerror(int code);
void infra_set_error(int code, const char* msg);
const char* infra_get_error(void);
void* infra_malloc(size_t size);
void* infra_calloc(size_t nmemb, size_t size);
void* infra_realloc(void* ptr, size_t size);
void infra_free(void* ptr);
void infra_get_stats(struct infra_stats* stats);
void infra_reset_stats(void);

/* Linked List */
struct infra_list {
    struct infra_list* next;
    struct infra_list* prev;
};

void infra_list_init(struct infra_list* list);
void infra_list_add(struct infra_list* list, struct infra_list* node);
void infra_list_del(struct infra_list* node);
int infra_list_empty(struct infra_list* list);

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

/* Queue */
struct infra_queue_node {
    struct infra_list list;
    void* data;
};

struct infra_queue {
    struct infra_list list;
    size_t size;
};

void infra_queue_init(struct infra_queue* queue);
int infra_queue_push(struct infra_queue* queue, void* data);
void* infra_queue_pop(struct infra_queue* queue);
int infra_queue_empty(struct infra_queue* queue);
size_t infra_queue_size(struct infra_queue* queue);

/* Red-Black Tree */
#define INFRA_RB_RED   0
#define INFRA_RB_BLACK 1

struct infra_rb_node {
    struct infra_rb_node* parent;
    struct infra_rb_node* left;
    struct infra_rb_node* right;
    int color;
};

struct infra_rb_tree {
    struct infra_rb_node* root;
    size_t size;
};

void infra_rbtree_init(struct infra_rb_tree* tree);
int infra_rbtree_insert(struct infra_rb_tree* tree, struct infra_rb_node* node,
                       int (*cmp)(struct infra_rb_node*, struct infra_rb_node*));
struct infra_rb_node* infra_rbtree_find(struct infra_rb_tree* tree, struct infra_rb_node* key,
                                       int (*cmp)(struct infra_rb_node*, struct infra_rb_node*));
size_t infra_rbtree_size(struct infra_rb_tree* tree);

// Event types
#define INFRA_EVENT_READ  0x01
#define INFRA_EVENT_WRITE 0x02
#define INFRA_EVENT_ERROR 0x04

// Timer wheel constants
#define INFRA_TIMER_WHEEL_BITS  8
#define INFRA_TIMER_WHEEL_SIZE  (1 << INFRA_TIMER_WHEEL_BITS)
#define INFRA_TIMER_WHEEL_MASK  (INFRA_TIMER_WHEEL_SIZE - 1)
#define INFRA_TIMER_WHEEL_COUNT 4

// Forward declarations
struct infra_event_loop;
struct infra_event;
struct infra_timer;

// Event handler callback
typedef void (*infra_event_handler)(struct infra_event* event, uint32_t events);

// Timer callback
typedef void (*infra_timer_handler)(struct infra_timer* timer, void* user_data);

// Event structure
typedef struct infra_event {
    int fd;                     // File descriptor
    uint32_t events;           // Registered events
    infra_event_handler handler; // Event handler callback
    void* user_data;           // User data
    struct infra_event* next;   // Next event in list
} infra_event_t;

// Timer structure
typedef struct infra_timer {
    uint64_t interval_ms;      // Timer interval in milliseconds
    uint64_t next_timeout;     // Next timeout in microseconds
    infra_timer_handler callback; // Timer callback
    void* user_data;           // User data
    struct infra_timer* next;   // Next timer in wheel slot
    bool repeating;            // Whether timer repeats
    struct {
        uint64_t total_calls;   // Total number of timer callbacks
        uint64_t total_elapsed; // Total elapsed time
        uint64_t min_elapsed;   // Minimum elapsed time
        uint64_t max_elapsed;   // Maximum elapsed time
        uint64_t last_elapsed;  // Last elapsed time
        uint64_t drift;        // Total drift from expected intervals
    } stats;
} infra_timer_t;

// Timer wheel structure
typedef struct infra_timer_wheel {
    infra_timer_t* slots[INFRA_TIMER_WHEEL_SIZE];
    uint32_t current;
} infra_timer_wheel_t;

// Event loop structure
typedef struct infra_event_loop {
    bool running;              // Whether loop is running
    infra_event_t* events;     // List of registered events
    size_t event_count;        // Number of registered events
    int epoll_fd;             // epoll file descriptor
    void* kqueue_fd;          // kqueue file descriptor (for BSD)
    void* iocp_handle;        // IOCP handle (for Windows)
    infra_timer_wheel_t wheels[INFRA_TIMER_WHEEL_COUNT];
    uint64_t current_time;    // Current time in microseconds
    uint64_t start_time;      // Start time in microseconds
    size_t total_timers;      // Total number of timers created
    size_t active_timers;     // Number of active timers
    size_t expired_timers;    // Number of expired timers
    size_t overdue_timers;    // Number of overdue timers
    uint64_t total_drift;     // Total timer drift
} infra_event_loop_t;

// Event loop functions
int infra_event_loop_create(infra_event_loop_t** loop);
int infra_event_loop_destroy(infra_event_loop_t* loop);
int infra_event_loop_run(infra_event_loop_t* loop, int timeout_ms);

// Event functions
int infra_event_add(infra_event_loop_t* loop, infra_event_t* event);
int infra_event_remove(infra_event_loop_t* loop, infra_event_t* event);
int infra_event_modify(infra_event_loop_t* loop, infra_event_t* event);

// Timer functions
int infra_timer_create(infra_event_loop_t* loop, infra_timer_t** timer, uint64_t interval_ms);
int infra_timer_destroy(infra_event_loop_t* loop, infra_timer_t* timer);
int infra_timer_start(infra_event_loop_t* loop, infra_timer_t* timer, bool repeating);
int infra_timer_stop(infra_event_loop_t* loop, infra_timer_t* timer);

// IO functions
int infra_io_read(int fd, void* buf, size_t count);
int infra_io_write(int fd, const void* buf, size_t count);
int infra_io_read_async(infra_event_loop_t* loop, int fd, void* buf, size_t count, infra_event_handler handler);
int infra_io_write_async(infra_event_loop_t* loop, int fd, const void* buf, size_t count, infra_event_handler handler);

#endif // PPDB_INFRA_H
