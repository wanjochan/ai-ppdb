#include "ppdb/async.h"

/*
 * I/O Backend Abstraction Layer
 * 
 * Current implementation: poll-based I/O multiplexing
 * 
 * Future upgrade paths:
 * 1. Windows: IOCP (I/O Completion Ports)
 *    - Requires overlapped I/O support
 *    - Better thread pool integration
 *    - More efficient for large number of connections
 * 
 * 2. Linux: epoll
 *    - Better scalability for large number of fds
 *    - Edge-triggered notification support
 *    - More efficient memory usage
 * 
 * 3. BSD/macOS: kqueue
 *    - Similar benefits to epoll
 *    - Unified interface for various event types
 * 
 * Implementation considerations for future:
 * - Abstract completion queue interface
 * - Thread pool management
 * - Zero-copy optimization
 * - Batch operation support
 */

// I/O 多路复用机制的抽象
typedef struct io_backend_s {
    // 初始化后端
    int (*init)(void* data);
    // 注册文件描述符
    int (*register_fd)(void* data, int fd, int events);
    // 修改文件描述符的监听事件
    int (*modify_fd)(void* data, int fd, int events);
    // 取消注册文件描述符
    int (*unregister_fd)(void* data, int fd);
    // 等待事件
    int (*wait)(void* data, int timeout_ms);
    // 获取就绪事件
    int (*get_event)(void* data, size_t idx, int* fd, int* events);
    // 清理后端
    void (*cleanup)(void* data);
    // 后端私有数据
    void* data;
    
    // Future extension points:
    void (*on_thread_start)(void* data);  /* 线程池支持 */
    void (*on_thread_stop)(void* data);   /* 线程池支持 */
    int (*batch_submit)(void* data, void* batch, size_t count);  /* 批量操作支持 */
    int (*zero_copy_send)(void* data, int fd, void* buf, size_t len);  /* 零拷贝支持 */
    void* reserved[4];  /* 为将来扩展预留空间 */
} io_backend_t;

// Poll 后端的具体实现
typedef struct poll_backend_s {
    struct pollfd* fds;
    size_t size;
    size_t capacity;
    // TODO: 添加线程池支持
    // TODO: 添加完成队列
} poll_backend_t;

static int poll_init(void* data) {
    // TODO: Windows下考虑使用IOCP初始化
    // TODO: Linux下考虑使用epoll_create
    poll_backend_t* backend = data;
    backend->capacity = 16;
    backend->size = 0;
    backend->fds = calloc(backend->capacity, sizeof(struct pollfd));
    return backend->fds ? 0 : -1;
}

static int poll_register_fd(void* data, int fd, int events) {
    // TODO: Windows下改用CreateIoCompletionPort
    // TODO: Linux下改用epoll_ctl(EPOLL_CTL_ADD)
    poll_backend_t* backend = data;
    if (backend->size >= backend->capacity) {
        size_t new_cap = backend->capacity * 2;
        void* new_fds = realloc(backend->fds, new_cap * sizeof(struct pollfd));
        if (!new_fds) return -1;
        backend->fds = new_fds;
        backend->capacity = new_cap;
    }
    
    backend->fds[backend->size].fd = fd;
    backend->fds[backend->size].events = events;
    backend->size++;
    return 0;
}

static int poll_modify_fd(void* data, int fd, int events) {
    poll_backend_t* backend = data;
    for (size_t i = 0; i < backend->size; i++) {
        if (backend->fds[i].fd == fd) {
            backend->fds[i].events = events;
            return 0;
        }
    }
    return -1;
}

static int poll_unregister_fd(void* data, int fd) {
    poll_backend_t* backend = data;
    for (size_t i = 0; i < backend->size; i++) {
        if (backend->fds[i].fd == fd) {
            backend->size--;
            if (i < backend->size) {
                backend->fds[i] = backend->fds[backend->size];
            }
            return 0;
        }
    }
    return -1;
}

static int poll_wait(void* data, int timeout_ms) {
    // TODO: Windows下改用GetQueuedCompletionStatus
    // TODO: Linux下改用epoll_wait
    poll_backend_t* backend = data;
    return poll(backend->fds, backend->size, timeout_ms);
}

static int poll_get_event(void* data, size_t idx, int* fd, int* events) {
    // TODO: Windows下改用OVERLAPPED结构获取结果
    // TODO: Linux下改用epoll_event结构获取结果
    poll_backend_t* backend = data;
    if (idx >= backend->size) return -1;
    *fd = backend->fds[idx].fd;
    *events = backend->fds[idx].revents;
    return 0;
}

static void poll_cleanup(void* data) {
    poll_backend_t* backend = data;
    free(backend->fds);
    backend->fds = NULL;
    backend->size = backend->capacity = 0;
}

// 创建 poll 后端
static io_backend_t* create_poll_backend(void) {
    io_backend_t* backend = calloc(1, sizeof(io_backend_t));
    poll_backend_t* poll_data = calloc(1, sizeof(poll_backend_t));
    if (!backend || !poll_data) {
        free(backend);
        free(poll_data);
        return NULL;
    }
    
    backend->init = poll_init;
    backend->register_fd = poll_register_fd;
    backend->modify_fd = poll_modify_fd;
    backend->unregister_fd = poll_unregister_fd;
    backend->wait = poll_wait;
    backend->get_event = poll_get_event;
    backend->cleanup = poll_cleanup;
    backend->data = poll_data;
    
    if (backend->init(backend->data) < 0) {
        free(poll_data);
        free(backend);
        return NULL;
    }
    
    return backend;
}

struct async_handle_s {
    async_loop_t* loop;
    int fd;
    void* buf;
    size_t len;
    async_cb cb;
    int active;
};

struct async_loop_s {
    io_backend_t* backend;
    async_handle_t** handles;
    size_t size;
    size_t capacity;
};

async_loop_t* async_loop_new(void) {
    async_loop_t* loop = calloc(1, sizeof(*loop));
    if (!loop) return NULL;

    loop->capacity = 16;
    loop->handles = calloc(loop->capacity, sizeof(*loop->handles));
    loop->backend = create_poll_backend();
    
    if (!loop->handles || !loop->backend) {
        if (loop->backend) {
            loop->backend->cleanup(loop->backend->data);
            free(loop->backend->data);
            free(loop->backend);
        }
        free(loop->handles);
        free(loop);
        return NULL;
    }
    
    return loop;
}

void async_loop_free(async_loop_t* loop) {
    if (!loop) return;
    if (loop->backend) {
        loop->backend->cleanup(loop->backend->data);
        free(loop->backend->data);
        free(loop->backend);
    }
    free(loop->handles);
    free(loop);
}

async_handle_t* async_handle_new(async_loop_t* loop, int fd) {
    if (!loop || fd < 0) return NULL;
    
    if (loop->size >= loop->capacity) {
        size_t new_cap = loop->capacity * 2;
        void* new_handles = realloc(loop->handles, new_cap * sizeof(*loop->handles));
        if (!new_handles) return NULL;
        loop->handles = new_handles;
        loop->capacity = new_cap;
    }
    
    async_handle_t* handle = calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    
    handle->loop = loop;
    handle->fd = fd;
    
    if (loop->backend->register_fd(loop->backend->data, fd, 0) < 0) {
        free(handle);
        return NULL;
    }
    
    loop->handles[loop->size++] = handle;
    return handle;
}

void async_handle_free(async_handle_t* handle) {
    if (!handle) return;
    
    async_loop_t* loop = handle->loop;
    loop->backend->unregister_fd(loop->backend->data, handle->fd);
    
    for (size_t i = 0; i < loop->size; i++) {
        if (loop->handles[i] == handle) {
            loop->size--;
            if (i < loop->size) {
                loop->handles[i] = loop->handles[loop->size];
            }
            break;
        }
    }
    
    free(handle);
}

int async_handle_read(async_handle_t* handle, void* buf, size_t len, async_cb cb) {
    if (!handle || !buf || !cb) return -1;
    
    // TODO: Windows下使用WSARecv + OVERLAPPED
    // TODO: Linux下考虑使用边缘触发模式
    handle->buf = buf;
    handle->len = len;
    handle->cb = cb;
    handle->active = 1;
    
    return handle->loop->backend->modify_fd(
        handle->loop->backend->data, 
        handle->fd, 
        POLLIN
    );
}

int async_loop_run(async_loop_t* loop, int timeout_ms) {
    if (!loop) return -1;
    
    // TODO: 添加线程池支持
    // TODO: 添加批量操作支持
    int n = loop->backend->wait(loop->backend->data, timeout_ms);
    if (n <= 0) return n;
    
    for (size_t i = 0; i < loop->size; i++) {
        async_handle_t* handle = loop->handles[i];
        if (!handle->active) continue;
        
        int fd, events;
        if (loop->backend->get_event(loop->backend->data, i, &fd, &events) == 0) {
            if (events & POLLIN) {
                // TODO: Windows下使用IOCP完成回调
                // TODO: Linux下支持零拷贝操作
                ssize_t n = read(fd, handle->buf, handle->len);
                handle->active = 0;
                loop->backend->modify_fd(loop->backend->data, fd, 0);
                handle->cb(handle, n);
            }
        }
    }
    
    return n;
}
