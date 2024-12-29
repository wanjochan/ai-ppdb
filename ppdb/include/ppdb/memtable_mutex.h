#ifndef PPDB_MEMTABLE_MUTEX_H
#define PPDB_MEMTABLE_MUTEX_H

#include <cosmopolitan.h>
#include "ppdb/error.h"

// Memory table structure
typedef struct ppdb_memtable_t ppdb_memtable_t;

// Memory table iterator
typedef struct ppdb_memtable_iterator_t ppdb_memtable_iterator_t;

// Basic operations
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table);
void ppdb_memtable_destroy(ppdb_memtable_t* table);

ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len);

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len);

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const uint8_t* key, size_t key_len);

// Size management
size_t ppdb_memtable_size(ppdb_memtable_t* table);
size_t ppdb_memtable_max_size(ppdb_memtable_t* table);

// Copy data to a new memory table
ppdb_error_t ppdb_memtable_copy(ppdb_memtable_t* src, ppdb_memtable_t* dst);

// Iterator operations
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* table,
                                         ppdb_memtable_iterator_t** iter);
void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter);

bool ppdb_memtable_iterator_valid(ppdb_memtable_iterator_t* iter);
const uint8_t* ppdb_memtable_iterator_key(ppdb_memtable_iterator_t* iter);
const uint8_t* ppdb_memtable_iterator_value(ppdb_memtable_iterator_t* iter);
void ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter);

ppdb_error_t ppdb_memtable_iterator_get(ppdb_memtable_iterator_t* iter,
                                       const uint8_t** key, size_t* key_len,
                                       const uint8_t** value, size_t* value_len);

#endif // PPDB_MEMTABLE_MUTEX_H 