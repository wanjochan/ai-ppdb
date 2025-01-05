#include <ppdb/internal.h>

// Include implementation files
#include "base/base_memory.inc.c"
#include "base/base_error.inc.c"
#include "base/base_log.inc.c"
#include "base/base_context.inc.c"
#include "base/base_data.inc.c"
#include "base/base_cursor.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_skiplist.inc.c"
#include "base/base_api.inc.c"

ppdb_error_t ppdb_base_init(ppdb_base_t** base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    ppdb_base_t* b = (ppdb_base_t*)ppdb_aligned_alloc(16, sizeof(ppdb_base_t));
    if (!b) return PPDB_ERR_OUT_OF_MEMORY;

    b->initialized = true;
    b->reserved = NULL;

    *base = b;
    return PPDB_OK;
}

void ppdb_base_destroy(ppdb_base_t* base) {
    if (!base) return;
    if (base->initialized) {
        base->initialized = false;
        if (base->reserved) {
            ppdb_aligned_free(base->reserved);
        }
    }
    ppdb_aligned_free(base);
}
