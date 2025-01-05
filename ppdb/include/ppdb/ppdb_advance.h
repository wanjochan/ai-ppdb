#ifndef PPDB_ADVANCE_H
#define PPDB_ADVANCE_H

#include "ppdb/ppdb.h"

//预留 计划 高级特性

// Advanced operations
ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_metrics_t* metrics);
ppdb_error_t ppdb_storage_get_ops(ppdb_base_t* base, ppdb_advance_ops_t* ops);

#endif // PPDB_ADVANCE_H
