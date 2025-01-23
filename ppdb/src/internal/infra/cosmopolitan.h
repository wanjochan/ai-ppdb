#ifndef COSMOPOLITAN_H_
#define COSMOPOLITAN_H_

/* Include the core cosmopolitan header */
#include <cosmo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>     /* 信号处理 */
#include <pthread.h>    /* 线程支持 */
#include <stdatomic.h>  /* 原子操作 */
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

/* epoll 相关定义 */
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN        0x001
#define EPOLLPRI       0x002
#define EPOLLOUT       0x004
#define EPOLLRDNORM    0x040
#define EPOLLRDBAND    0x080
#define EPOLLWRNORM    0x100
#define EPOLLWRBAND    0x200
#define EPOLLMSG       0x400
#define EPOLLERR       0x008
#define EPOLLHUP       0x010
#define EPOLLRDHUP     0x2000
#define EPOLLEXCLUSIVE (1U << 28)
#define EPOLLWAKEUP    (1U << 29)
#define EPOLLONESHOT   (1U << 30)
#define EPOLLET        (1U << 31)

typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t events;
    epoll_data_t data;
} __attribute__ ((__packed__));

/* epoll 函数声明 */
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

/* 基础类型定义 */
typedef int8_t          i8;
typedef uint8_t         u8;
typedef int16_t         i16;
typedef uint16_t        u16;
typedef int32_t         i32;
typedef uint32_t        u32;
typedef int64_t         i64;
typedef uint64_t        u64;
typedef float           f32;
typedef double          f64;
typedef size_t          usize;
typedef ssize_t         isize;
typedef uintptr_t       uptr;
typedef intptr_t        iptr;

/* 错误处理宏 */
#define HANDLE_ERROR(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define HANDLE_ERROR_EN(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define CHECK_ERROR(ret) \
    do { if ((ret) == -1) HANDLE_ERROR(#ret); } while (0)

/* Thread-safe error handling */
#define THREAD_ERROR_CHECK(status, msg) \
    do { \
        int _err = (status); \
        if (_err != 0) { \
            errno = _err; \
            HANDLE_ERROR(msg); \
        } \
    } while (0)

/* 常用宏定义 */
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#define container_of(ptr, type, member) ({ \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) );})

/* 内存对齐宏 */
#define ALIGN_UP(x, align) (((x) + ((align)-1)) & ~((align)-1))
#define ALIGN_DOWN(x, align) ((x) & ~((align)-1))
#define IS_ALIGNED(x, align) (((x) & ((align)-1)) == 0)
#define ROUND_UP(x, align) ALIGN_UP(x, align)
#define ROUND_DOWN(x, align) ALIGN_DOWN(x, align)

/* 位操作宏 */
#define BIT(nr)                 (1UL << (nr))
#define BIT_SET(nr, addr)      (*addr |= BIT(nr))
#define BIT_CLR(nr, addr)      (*addr &= ~BIT(nr))
#define BIT_TEST(nr, addr)     ((*addr) & BIT(nr))
#define BIT_MASK(nr)           (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)           ((nr) / BITS_PER_LONG)
#define BITS_PER_BYTE          8
#define BITS_PER_LONG          (sizeof(long) * BITS_PER_BYTE)

/* 编译器相关宏 */
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#define prefetch(x)    __builtin_prefetch(x)
#define UNUSED(x)      (void)(x)
#define PACKED         __attribute__((packed))
#define ALIGNED(x)     __attribute__((aligned(x)))
#define NORETURN       __attribute__((noreturn))
#define WEAK           __attribute__((weak))
#define PURE          __attribute__((pure))
#define CONST         __attribute__((const))

/* 调试辅助宏 */
#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) \
    fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
            __LINE__, __func__, __VA_ARGS__)
#define ASSERT(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: Assertion `%s' failed.\n", \
                    __FILE__, __LINE__, #x); \
            abort(); \
        } \
    } while (0)
#else
#define DEBUG_PRINT(fmt, ...) /* nothing */
#define ASSERT(x) /* nothing */
#endif

/* 内存屏障 */
#define barrier() __asm__ __volatile__("": : :"memory")
#define mb()  __sync_synchronize()
#define rmb() __sync_synchronize()
#define wmb() __sync_synchronize()
#define smp_mb()  barrier()
#define smp_rmb() barrier()
#define smp_wmb() barrier()

/* 原子操作辅助宏 */
#define atomic_inc(v)          __sync_add_and_fetch(v, 1)
#define atomic_dec(v)          __sync_sub_and_fetch(v, 1)
#define atomic_add(v, n)       __sync_add_and_fetch(v, n)
#define atomic_sub(v, n)       __sync_sub_and_fetch(v, n)
#define atomic_set(v, n)       __sync_lock_test_and_set(v, n)
#define atomic_read(v)         __sync_fetch_and_add(v, 0)

#endif /* COSMOPOLITAN_H_ */