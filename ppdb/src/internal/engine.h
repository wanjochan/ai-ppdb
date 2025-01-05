#ifndef PPDB_INTERNAL_ENGINE_H
#define PPDB_INTERNAL_ENGINE_H

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include <ppdb/internal/base.h>
#include <ppdb/internal/error.h>

// Engine layer error codes (0x2000-0x2FFF)
#define PPDB_ENGINE_ERR_INIT       (PPDB_ENGINE_ERROR_START + 0x001)
#define PPDB_ENGINE_ERR_PARAM      (PPDB_ENGINE_ERROR_START + 0x002)
#define PPDB_ENGINE_ERR_MUTEX      (PPDB_ENGINE_ERROR_START + 0x003)
#define PPDB_ENGINE_ERR_TXN        (PPDB_ENGINE_ERROR_START + 0x004)
#define PPDB_ENGINE_ERR_MVCC       (PPDB_ENGINE_ERROR_START + 0x005)
#define PPDB_ENGINE_ERR_ASYNC      (PPDB_ENGINE_ERROR_START + 0x006)
#define PPDB_ENGINE_ERR_TIMEOUT    (PPDB_ENGINE_ERROR_START + 0x007)
#define PPDB_ENGINE_ERR_BUSY       (PPDB_ENGINE_ERROR_START + 0x008)
#define PPDB_ENGINE_ERR_FULL       (PPDB_ENGINE_ERROR_START + 0x009)
#define PPDB_ENGINE_ERR_NOT_FOUND  (PPDB_ENGINE_ERROR_START + 0x00A)
#define PPDB_ENGINE_ERR_EXISTS     (PPDB_ENGINE_ERROR_START + 0x00B)

// Error message conversion function
const char* ppdb_engine_strerror(ppdb_error_t err);

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