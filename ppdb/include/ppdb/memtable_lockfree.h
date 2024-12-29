#ifndef PPDB_MEMTABLE_LOCKFREE_H
#define PPDB_MEMTABLE_LOCKFREE_H

#include <cosmopolitan.h>
#include "ppdb/error.h"

// Lock-free memory table structure
typedef struct ppdb_memtable_t ppdb_memtable_t;

// Basic operations
ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** table);
void ppdb_memtable_destroy_lockfree(ppdb_memtable_t* table);

ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table,
                                       const uint8_t* key, size_t key_len,
                                       const uint8_t* value, size_t value_len);

ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table,
                                       const uint8_t* key, size_t key_len,
                                       uint8_t** value, size_t* value_len);

ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table,
                                          const uint8_t* key, size_t key_len);

// Size management
size_t ppdb_memtable_size_lockfree(ppdb_memtable_t* table);
size_t ppdb_memtable_max_size_lockfree(ppdb_memtable_t* table);

#endif // PPDB_MEMTABLE_LOCKFREE_H 