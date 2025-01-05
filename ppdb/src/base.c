#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 错误处理实现
//-----------------------------------------------------------------------------

static const char* error_messages[] = {
    "成功",                     // PPDB_OK
    "空指针",                   // PPDB_ERR_NULL_POINTER
    "内存不足",                 // PPDB_ERR_OUT_OF_MEMORY
    "未找到",                   // PPDB_ERR_NOT_FOUND
    "已存在",                   // PPDB_ERR_ALREADY_EXISTS
    "无效类型",                 // PPDB_ERR_INVALID_TYPE
    "无效状态",                 // PPDB_ERR_INVALID_STATE
    "内部错误",                 // PPDB_ERR_INTERNAL
    "不支持",                   // PPDB_ERR_NOT_SUPPORTED
    "存储已满",                 // PPDB_ERR_FULL
    "存储为空",                 // PPDB_ERR_EMPTY
    "数据损坏",                 // PPDB_ERR_CORRUPTED
    "IO错误",                   // PPDB_ERR_IO
    "资源忙",                   // PPDB_ERR_BUSY
    "超时",                     // PPDB_ERR_TIMEOUT
};

const char* ppdb_strerror(ppdb_error_t err) {
    if (err == 0) return error_messages[0];
    if (err < -33 || err > 0) return "未知错误";
    return error_messages[-err];
}

//-----------------------------------------------------------------------------
// 内存管理实现
//-----------------------------------------------------------------------------

void* aligned_alloc(size_t alignment, size_t size) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
}

void aligned_free(void* ptr) {
    free(ptr);
}

//-----------------------------------------------------------------------------
// 随机数生成器实现
//-----------------------------------------------------------------------------

static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

void ppdb_random_init(ppdb_random_state_t* state, uint64_t seed) {
    // 使用 SplitMix64 生成初始种子
    uint64_t z = (seed + 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    state->s[0] = z ^ (z >> 31);

    z = (state->s[0] + 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    state->s[1] = z ^ (z >> 31);

    z = (state->s[1] + 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    state->s[2] = z ^ (z >> 31);

    z = (state->s[2] + 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    state->s[3] = z ^ (z >> 31);
}

uint64_t ppdb_random_next(ppdb_random_state_t* state) {
    const uint64_t result = rotl(state->s[1] * 5, 7) * 9;
    const uint64_t t = state->s[1] << 17;

    state->s[2] ^= state->s[0];
    state->s[3] ^= state->s[1];
    state->s[1] ^= state->s[2];
    state->s[0] ^= state->s[3];

    state->s[2] ^= t;
    state->s[3] = rotl(state->s[3], 45);

    return result;
}

double ppdb_random_double(ppdb_random_state_t* state) {
    // 生成 [0, 1) 范围的双精度浮点数
    const uint64_t value = ppdb_random_next(state);
    // 使用高53位来构造双精度浮点数
    const uint64_t mask = (1ULL << 53) - 1;
    return (value & mask) * (1.0 / (1ULL << 53));
}

//-----------------------------------------------------------------------------
// 同步原语实现
//-----------------------------------------------------------------------------

#include "base_sync.inc.c"

//-----------------------------------------------------------------------------
// 配置验证实现
//-----------------------------------------------------------------------------

ppdb_error_t validate_and_setup_config(ppdb_config_t* config) {
    if (!config) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // 验证存储类型
    if (config->type == 0) {
        config->type = PPDB_MEMKV_DEFAULT;
    }

    // 验证分片数量
    if (config->shard_count == 0) {
        config->shard_count = DEFAULT_SHARD_COUNT;
    }

    // 验证内存限制
    if (config->memory_limit == 0) {
        config->memory_limit = DEFAULT_MEMTABLE_SIZE;
    }

    return PPDB_OK;
}
