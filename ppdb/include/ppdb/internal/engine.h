#ifndef PPDB_INTERNAL_ENGINE_H
#define PPDB_INTERNAL_ENGINE_H

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include <ppdb/internal/base.h>

// Forward declaration for engine only
typedef struct ppdb_engine_s ppdb_engine_t;

// Engine type
struct ppdb_engine_s {
    ppdb_base_t* base;                // Using ppdb_base_t from base.h
    ppdb_base_mutex_t* txn_mutex;     // Using ppdb_base_mutex_t from base.h
};

// Engine-specific function declarations
ppdb_error_t ppdb_engine_init(ppdb_engine_t** engine, ppdb_base_t* base);
void ppdb_engine_destroy(ppdb_engine_t* engine);

#endif // PPDB_INTERNAL_ENGINE_H