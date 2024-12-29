#ifndef PPDB_WAL_MUTEX_H
#define PPDB_WAL_MUTEX_H

#include <cosmopolitan.h>
#include "ppdb/error.h"
#include "ppdb/memtable.h"

// WAL magic number and version
#define WAL_MAGIC 0x4C415750  // "PWAL"
#define WAL_VERSION 1

// Record types
typedef enum {
    PPDB_WAL_RECORD_PUT = 1,
    PPDB_WAL_RECORD_DELETE = 2
} ppdb_wal_record_type_t;

// WAL configuration
typedef struct {
    const char* dir_path;     // WAL directory path
    size_t segment_size;      // Size of each segment
    bool sync_write;          // Whether to sync after each write
} ppdb_wal_config_t;

// WAL structure
typedef struct ppdb_wal_t ppdb_wal_t;

// Basic operations
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy(ppdb_wal_t* wal);
void ppdb_wal_close(ppdb_wal_t* wal);

// Write a record to WAL
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal,
                           ppdb_wal_record_type_t type,
                           const void* key,
                           size_t key_size,
                           const void* value,
                           size_t value_size);

// Recover data from WAL files
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t** table);

// Archive old WAL files
ppdb_error_t ppdb_wal_archive(ppdb_wal_t* wal);

#endif // PPDB_WAL_MUTEX_H 