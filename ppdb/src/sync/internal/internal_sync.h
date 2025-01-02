#ifndef PPDB_INTERNAL_SYNC_H
#define PPDB_INTERNAL_SYNC_H

#include <cosmopolitan.h>
#include "ppdb/sync.h"

// 内部函数声明
ppdb_error_t ppdb_sync_retry(ppdb_sync_t* sync,
                            ppdb_sync_config_t* config,
                            ppdb_error_t (*retry_func)(void*),
                            void* args);

// 内部工具函数
void ppdb_sync_pause(void);
void ppdb_sync_backoff(uint32_t backoff_us);
bool ppdb_sync_should_yield(uint32_t spin_count, uint32_t current_spins);

#endif // PPDB_INTERNAL_SYNC_H