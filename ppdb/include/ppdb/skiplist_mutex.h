#ifndef PPDB_SKIPLIST_MUTEX_H
#define PPDB_SKIPLIST_MUTEX_H

#include <cosmopolitan.h>
#include "ppdb/error.h"

// Skip list structure
typedef struct skiplist_t skiplist_t;

// Basic operations
skiplist_t* skiplist_create(void);
void skiplist_destroy(skiplist_t* list);

ppdb_error_t skiplist_put(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                const uint8_t* value, size_t value_len);

ppdb_error_t skiplist_get(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                uint8_t* value, size_t* value_len);

ppdb_error_t skiplist_delete(skiplist_t* list,
                   const uint8_t* key, size_t key_len);

size_t skiplist_size(skiplist_t* list);

// Iterator interface
typedef struct skiplist_iterator_t skiplist_iterator_t;

skiplist_iterator_t* skiplist_iterator_create(skiplist_t* list);
void skiplist_iterator_destroy(skiplist_iterator_t* iter);

// Returns false when iteration is complete
bool skiplist_iterator_next(skiplist_iterator_t* iter,
                          uint8_t** key, size_t* key_size,
                          uint8_t** value, size_t* value_size);

#endif // PPDB_SKIPLIST_MUTEX_H 