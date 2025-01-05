#include <ppdb/internal.h>

// Include all base implementation files
#include "base/base_memory.inc.c"
#include "base/base_context.inc.c"
#include "base/base_data.inc.c"
#include "base/base_cursor.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_log.inc.c"
#include "base/base_skiplist.inc.c"

ppdb_error_t ppdb_base_init(ppdb_base_t** base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    if (*base) return PPDB_ERR_EXISTS;

    ppdb_base_t* b = (ppdb_base_t*)ppdb_aligned_alloc(sizeof(ppdb_base_t));
    if (!b) return PPDB_ERR_OUT_OF_MEMORY;

    // Initialize base structure
    memset(b, 0, sizeof(ppdb_base_t));

    // Create memory pool
    ppdb_error_t err = ppdb_mempool_create(&b->pool, 1024, 16);
    if (err != PPDB_OK) {
        ppdb_aligned_free(b);
        return err;
    }

    // Create root context
    ppdb_ctx_t ctx_handle;
    err = ppdb_context_create(&ctx_handle);
    if (err != PPDB_OK) {
        ppdb_mempool_destroy(b->pool);
        ppdb_aligned_free(b);
        return err;
    }
    b->root_context = ppdb_context_get(ctx_handle);

    *base = b;
    return PPDB_OK;
}

void ppdb_base_destroy(ppdb_base_t* base) {
    if (!base) return;

    // Cleanup in reverse order
    if (base->root_context) {
        ppdb_ctx_t ctx_handle = (ppdb_ctx_t)((ppdb_context_internal_t*)base->root_context - offsetof(ppdb_context_internal_t, ctx))->id;
        ppdb_context_destroy(ctx_handle);
    }

    if (base->pool) {
        ppdb_mempool_destroy(base->pool);
    }

    ppdb_aligned_free(base);
}
