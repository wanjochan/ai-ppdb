#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include <cosmopolitan.h>

#ifndef PPDB_STORAGE_C
#define PPDB_STORAGE_C

void cleanup_base(ppdb_base_t* base) {
    if (!base) {
        return;
    }

    if (base->shards) {
        for (uint32_t i = 0; i < base->config.shard_count; i++) {
            ppdb_shard_t* shard = &base->shards[i];
            if (shard->head) {
                node_unref(shard->head);
            }
            if (shard->lock) {
                ppdb_sync_destroy(shard->lock);
                PPDB_ALIGNED_FREE(shard->lock);
            }
        }
        PPDB_ALIGNED_FREE(base->shards);
    }
}

//
#include "storage_crud.inc.c"
//
#include "storage_iterator.inc.c"
//
#include "storage_misc.inc.c"

#endif // PPDB_STORAGE_C