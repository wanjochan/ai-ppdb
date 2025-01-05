#ifndef PPDB_INTERNAL_H
#define PPDB_INTERNAL_H

// 公共API
#include "ppdb.h"

// 内部实现
#include "../internal/base.h"  // 基础工具
#include "../internal/core.h"  // 核心实现

// 内部使用的宏
#define PPDB_INTERNAL_ASSERT(x) do { \
    if (!(x)) { \
        ppdb_log(PPDB_LOG_FATAL, "Assertion failed: %s", #x); \
        abort(); \
    } \
} while (0)

#define PPDB_INTERNAL_CHECK(x) do { \
    ppdb_error_t err = (x); \
    if (err != PPDB_OK) { \
        return err; \
    } \
} while (0)

#endif // PPDB_INTERNAL_H
