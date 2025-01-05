#ifndef PPDB_INTERNAL_CORE_H
#define PPDB_INTERNAL_CORE_H

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>

// Forward declarations
typedef struct ppdb_core_mutex_s ppdb_core_mutex_t;
typedef struct ppdb_base_s ppdb_base_t;
typedef struct ppdb_core_s ppdb_core_t;

// Core mutex type
struct ppdb_core_mutex_s {
    pthread_mutex_t mutex;
    bool initialized;
};

// Base type
struct ppdb_base_s {
    bool initialized;
    void* reserved;
    ppdb_core_mutex_t* mutex;
};

// Core type
struct ppdb_core_s {
    ppdb_base_t* base;
    ppdb_core_mutex_t* txn_mutex;
};

// Function declarations
ppdb_error_t ppdb_core_init(ppdb_core_t** core, ppdb_base_t* base);
void ppdb_core_destroy(ppdb_core_t* core);

ppdb_error_t ppdb_core_mutex_create(ppdb_core_mutex_t** mutex);
void ppdb_core_mutex_destroy(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_lock(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_unlock(ppdb_core_mutex_t* mutex);

#endif // PPDB_INTERNAL_CORE_H 