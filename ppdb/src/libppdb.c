//=============================================================================
// PPDB Library Implementation
//
// 注意事项：
// 1. 本文件实现了PPDB的核心功能，可以被编译成共享库供其他程序使用，也可以直接被主程序调用（编译成.o）
// 2. 共享库构建目标：
//    - Windows: libppdb.dll (需要使用mingw64构建，当前cross9不支持)
//    - Linux:   libppdb.so
//    - macOS:   libppdb.dyld
// 3. 使用方法：
//    - 包含ppdb.h头文件
//    - 链接对应平台的共享库
//    - 按照头文件中定义的接口进行调用
//
// 更多信息请参考：docs/ARCHITECTURE.md
//=============================================================================
// PPDB Library Implementation
//
// TODO: We plan to build libppdb.dat using Cosmopolitan mechanism to achieve 
// a cross-platform callable dynamic library. Implementation date TBD.
//
// Current build targets:
// - Windows: libppdb.dll (requires mingw64, not supported by current cross9)
// - Linux: libppdb.so
// - macOS: libppdb.dyld
//
// Usage:
// - Include ppdb.h header
// - Link against platform-specific shared library
// - Call APIs defined in header
//
// See docs/ARCHITECTURE.md for more details

//=============================================================================

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/database.h"
#include "internal/peer.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

typedef struct ppdb_ctx_s {
    ppdb_options_t options;
    bool initialized;
} ppdb_ctx_s;

//-----------------------------------------------------------------------------
// Context Management
//-----------------------------------------------------------------------------

// Create database context
ppdb_error_t ppdb_create(ppdb_ctx_t** ctx, const ppdb_options_t* options) {
    if (!ctx || !options) {
        return PPDB_ERR_PARAM;
    }
    
    // Allocate context
    ppdb_ctx_t* context = (ppdb_ctx_t*)malloc(sizeof(ppdb_ctx_t));
    if (!context) {
        return PPDB_ERR_MEMORY;
    }
    
    // Initialize context
    memset(context, 0, sizeof(ppdb_ctx_t));
    context->initialized = true;
    context->options = *options;
    
    *ctx = context;
    return PPDB_OK;
}

// Destroy database context
void ppdb_destroy(ppdb_ctx_t* ctx) {
    if (!ctx) return;
    
    // Stop and destroy server if running
    if (ctx->server) {
        ppdb_server_stop(ctx->server);
        ppdb_server_destroy(ctx->server);
    }
    
    // Free context
    free(ctx);
}

// Library initialization
ppdb_error_t ppdb_init(void) {
    // Initialize base layer
    ppdb_error_t err = ppdb_base_init();
    if (err != PPDB_OK) {
        return err;
    }

    // Initialize database layer
    err = ppdb_database_init();
    if (err != PPDB_OK) {
        ppdb_base_cleanup();
        return err;
    }

    // Initialize peer layer
    err = ppdb_peer_init();
    if (err != PPDB_OK) {
        ppdb_database_cleanup();
        ppdb_base_cleanup();
        return err;
    }

    return PPDB_OK;
}

// Library cleanup
void ppdb_cleanup(void) {
    ppdb_peer_cleanup();
    ppdb_database_cleanup();
    ppdb_base_cleanup();
} 