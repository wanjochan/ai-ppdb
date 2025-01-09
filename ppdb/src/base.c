/*
 * base.c - Base Layer Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Include implementation files
#include "base/base_core.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_struct.inc.c"
#include "base/base_net.inc.c"

// Global variables
static ppdb_base_t* g_base = NULL;

// Mutex statistics functions
void ppdb_base_mutex_enable_stats(ppdb_base_mutex_t* mutex, bool enable) {
    if (!mutex) return;
    mutex->stats.enabled = enable;
    mutex->stats.contention = 0;
}
