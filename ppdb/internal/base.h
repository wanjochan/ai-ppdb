#ifndef PPDB_INTERNAL_BASE_H
#define PPDB_INTERNAL_BASE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

//-----------------------------------------------------------------------------
// 内存管理
//-----------------------------------------------------------------------------

// 内存分配
void* ppdb_malloc(size_t size);
void* ppdb_calloc(size_t count, size_t size);
void* ppdb_realloc(void* ptr, size_t size);
void ppdb_free(void* ptr);

// 内存池
typedef struct ppdb_mempool ppdb_mempool_t;
ppdb_mempool_t* ppdb_mempool_create(size_t block_size, size_t block_count);
void ppdb_mempool_destroy(ppdb_mempool_t* pool);
void* ppdb_mempool_alloc(ppdb_mempool_t* pool);
void ppdb_mempool_free(ppdb_mempool_t* pool, void* ptr);

//-----------------------------------------------------------------------------
// 日志
//-----------------------------------------------------------------------------

typedef enum ppdb_log_level {
    PPDB_LOG_DEBUG,
    PPDB_LOG_INFO,
    PPDB_LOG_WARN,
    PPDB_LOG_ERROR,
    PPDB_LOG_FATAL
} ppdb_log_level_t;

void ppdb_log(ppdb_log_level_t level, const char* fmt, ...);
void ppdb_set_log_level(ppdb_log_level_t level);

//-----------------------------------------------------------------------------
// 时间
//-----------------------------------------------------------------------------

uint64_t ppdb_time_ms(void);
uint64_t ppdb_time_us(void);
void ppdb_sleep_ms(uint32_t ms);
void ppdb_sleep_us(uint32_t us);

//-----------------------------------------------------------------------------
// 工具函数
//-----------------------------------------------------------------------------

// 字符串操作
char* ppdb_strdup(const char* str);
bool ppdb_streq(const char* a, const char* b);
size_t ppdb_strlcpy(char* dst, const char* src, size_t size);

// 哈希函数
uint32_t ppdb_hash32(const void* data, size_t len);
uint64_t ppdb_hash64(const void* data, size_t len);

// 原子操作
#define PPDB_ATOMIC_INC(ptr) __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#define PPDB_ATOMIC_DEC(ptr) __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#define PPDB_ATOMIC_ADD(ptr, val) __atomic_add_fetch(ptr, val, __ATOMIC_SEQ_CST)
#define PPDB_ATOMIC_SUB(ptr, val) __atomic_sub_fetch(ptr, val, __ATOMIC_SEQ_CST)

// 位操作
#define PPDB_BIT_SET(x, b) ((x) |= (1ULL << (b)))
#define PPDB_BIT_CLR(x, b) ((x) &= ~(1ULL << (b)))
#define PPDB_BIT_TEST(x, b) ((x) & (1ULL << (b)))

// 对齐
#define PPDB_ALIGN_UP(x, a) (((x) + ((a)-1)) & ~((a)-1))
#define PPDB_ALIGN_DOWN(x, a) ((x) & ~((a)-1))

// 最大/最小值
#define PPDB_MIN(a, b) ((a) < (b) ? (a) : (b))
#define PPDB_MAX(a, b) ((a) > (b) ? (a) : (b))

#endif // PPDB_INTERNAL_BASE_H
