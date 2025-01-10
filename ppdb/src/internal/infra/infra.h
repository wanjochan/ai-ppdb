/*
 * infra.h - Core Infrastructure Definitions
 */

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

/* Utility Macros */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* Error Codes */
#define PPDB_OK           0
#define PPDB_ERR_PARAM    1
#define PPDB_ERR_MEMORY   2
#define PPDB_ERR_THREAD   3
#define PPDB_ERR_MUTEX    4
#define PPDB_ERR_COND     5
#define PPDB_ERR_RWLOCK   6
#define PPDB_ERR_BUSY     7
#define PPDB_ERR_NOTFOUND 8
#define PPDB_ERR_EXISTS   9
#define PPDB_ERR_IO       10
#define PPDB_ERR_TIMEOUT  11
#define PPDB_ERR_AGAIN    12
#define PPDB_ERR_INTR     13
#define PPDB_ERR_PERM     14
#define PPDB_ERR_NOENT    15
#define PPDB_ERR_NOSPC    16
#define PPDB_ERR_INVAL    17
#define PPDB_ERR_NFILE    18
#define PPDB_ERR_MFILE    19
#define PPDB_ERR_NOSYS    20
#define PPDB_ERR_PROTO    21
#define PPDB_ERR_NOBUFS   22
#define PPDB_ERR_NOMSG    23
#define PPDB_ERR_BADMSG   24
#define PPDB_ERR_OVERFLOW 25
#define PPDB_ERR_NODATA   26
#define PPDB_ERR_NOSR     27
#define PPDB_ERR_TIME     28
#define PPDB_ERR_NOLINK   29
#define PPDB_ERR_PROTO_NO_SUPPORT 30
#define PPDB_ERR_STALE    31
#define PPDB_ERR_UNREACH  32
#define PPDB_ERR_NOTCONN  33
#define PPDB_ERR_SHUTDOWN 34
#define PPDB_ERR_TOOBIG   35
#define PPDB_ERR_NOTDIR   36
#define PPDB_ERR_ISDIR    37
#define PPDB_ERR_INVALID  38
#define PPDB_ERR_NAMETOOLONG 39
#define PPDB_ERR_LOOP     40
#define PPDB_ERR_OPNOTSUPP 41
#define PPDB_ERR_ADDRINUSE 42
#define PPDB_ERR_ADDRNOTAVAIL 43
#define PPDB_ERR_NETDOWN  44
#define PPDB_ERR_NETUNREACH 45
#define PPDB_ERR_NETRESET 46
#define PPDB_ERR_CONNABORTED 47
#define PPDB_ERR_CONNRESET 48
#define PPDB_ERR_NOTSOCK  49
#define PPDB_ERR_DESTADDRREQ 50
#define PPDB_ERR_MSGSIZE  51
#define PPDB_ERR_PROTONOSUPPORT 52
#define PPDB_ERR_OPNOSUPPORT 53
#define PPDB_ERR_PFNOSUPPORT 54
#define PPDB_ERR_AFNOSUPPORT 55
#define PPDB_ERR_SOCKNOSUPPORT 56
#define PPDB_ERR_EOPNOTSUPP 57
#define PPDB_ERR_CRYPTOERR 58
#define PPDB_ERR_KEYEXPIRED 59
#define PPDB_ERR_KEYREVOKED 60
#define PPDB_ERR_KEYREJECTED 61
#define PPDB_ERR_OWNER_DIED 62
#define PPDB_ERR_STATE    63
#define PPDB_ERR_ATOMIC_BUSY 64
#define PPDB_ERR_REMOTE   65
#define PPDB_ERR_NOREMOTE 66
#define PPDB_ERR_RESTART  67
#define PPDB_ERR_TRYAGAIN 68
#define PPDB_ERR_IN_PROGRESS 69
#define PPDB_ERR_ALREADY  70
#define PPDB_ERR_LAST     71

typedef int ppdb_error_t;

/* Memory Management */
ppdb_error_t ppdb_mem_malloc(size_t size, void** ptr);
ppdb_error_t ppdb_mem_calloc(size_t nmemb, size_t size, void** ptr);
ppdb_error_t ppdb_mem_realloc(void* old_ptr, size_t size, void** new_ptr);
void ppdb_mem_free(void* ptr);

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

/* String Operations */
size_t infra_strlen(const char* str);
int infra_strcmp(const char* lhs, const char* rhs);
char* infra_strdup(const char* str);

/* Memory Operations */
void* infra_memset(void* dest, int ch, size_t count);
void* infra_memcpy(void* dest, const void* src, size_t count);
void* infra_memmove(void* dest, const void* src, size_t count);
int infra_memcmp(const void* lhs, const void* rhs, size_t count);

/* IO Operations */
int infra_printf(const char* format, ...);
int infra_dprintf(int fd, const char* format, ...);
int infra_puts(const char* str);
int infra_putchar(int ch);
int infra_io_read(int fd, void* buf, size_t count);
int infra_io_write(int fd, const void* buf, size_t count);

/* Buffer Operations */
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

#endif /* PPDB_INFRA_H */
