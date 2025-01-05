#ifndef PPDB_INTERNAL_ENGINE_H
#define PPDB_INTERNAL_ENGINE_H

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>

// Forward declarations
typedef struct ppdb_engine_mutex_s ppdb_engine_mutex_t;
typedef struct ppdb_base_s ppdb_base_t;
typedef struct ppdb_engine_s ppdb_engine_t;

// Engine mutex type
struct ppdb_engine_mutex_s {
    pthread_mutex_t mutex;
    bool initialized;
};

// Base type
struct ppdb_base_s {
    bool initialized;
    void* reserved;
    ppdb_engine_mutex_t* mutex;
};

// Engine type
struct ppdb_engine_s {
    ppdb_base_t* base;
    ppdb_engine_mutex_t* txn_mutex;
};

// Function declarations
ppdb_error_t ppdb_engine_init(ppdb_engine_t** engine, ppdb_base_t* base);
void ppdb_engine_destroy(ppdb_engine_t* engine);

ppdb_error_t ppdb_engine_mutex_create(ppdb_engine_mutex_t** mutex);
void ppdb_engine_mutex_destroy(ppdb_engine_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_lock(ppdb_engine_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_unlock(ppdb_engine_mutex_t* mutex);

#endif // PPDB_INTERNAL_ENGINE_H