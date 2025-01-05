#ifndef PPDB_BASE_SKIPLIST_INC_C
#define PPDB_BASE_SKIPLIST_INC_C

#include "ppdb/internal/base.h"
#include <cosmopolitan.h>

ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_data_t* key, const ppdb_data_t* value, uint32_t height) {
    if (height < 1 || height > MAX_SKIPLIST_LEVEL) {
        return NULL;
    }

    size_t node_size = sizeof(ppdb_node_t) + height * sizeof(ppdb_node_t*);
    ppdb_node_t* node = (ppdb_node_t*)ppdb_aligned_alloc(16, node_size);
    if (!node) {
        return NULL;
    }
    memset(node, 0, node_size);

    node->base = base;
    node->height = height;
    atomic_init(&node->state_machine.ref_count, 1);
    atomic_init(&node->state_machine.marked, false);

    if (key) {
        node->key = (ppdb_data_t*)ppdb_aligned_alloc(16, sizeof(ppdb_data_t));
        if (!node->key) {
            ppdb_aligned_free(node);
            return NULL;
        }
        memset(node->key, 0, sizeof(ppdb_data_t));
        
        // 复制内联数据
        memcpy(node->key->inline_data, key->inline_data, sizeof(node->key->inline_data));
        node->key->size = key->size;
        node->key->flags = key->flags;
        
        // 如果使用扩展数据，复制扩展数据
        if (key->flags & 1) {
            node->key->extended_data = ppdb_aligned_alloc(16, key->size);
            if (!node->key->extended_data) {
                ppdb_aligned_free(node->key);
                ppdb_aligned_free(node);
                return NULL;
            }
            memcpy(node->key->extended_data, key->extended_data, key->size);
        }
    }

    if (value) {
        node->value = (ppdb_data_t*)ppdb_aligned_alloc(16, sizeof(ppdb_data_t));
        if (!node->value) {
            if (node->key) {
                if (node->key->flags & 1) {
                    ppdb_aligned_free(node->key->extended_data);
                }
                ppdb_aligned_free(node->key);
            }
            ppdb_aligned_free(node);
            return NULL;
        }
        memset(node->value, 0, sizeof(ppdb_data_t));
        
        // 复制内联数据
        memcpy(node->value->inline_data, value->inline_data, sizeof(node->value->inline_data));
        node->value->size = value->size;
        node->value->flags = value->flags;
        
        // 如果使用扩展数据，复制扩展数据
        if (value->flags & 1) {
            node->value->extended_data = ppdb_aligned_alloc(16, value->size);
            if (!node->value->extended_data) {
                if (node->key) {
                    if (node->key->flags & 1) {
                        ppdb_aligned_free(node->key->extended_data);
                    }
                    ppdb_aligned_free(node->key);
                }
                ppdb_aligned_free(node->value);
                ppdb_aligned_free(node);
                return NULL;
            }
            memcpy(node->value->extended_data, value->extended_data, value->size);
        }
    }

    return node;
}

void node_ref(ppdb_node_t* node) {
    if (node) {
        atomic_fetch_add(&node->state_machine.ref_count, 1);
    }
}

void node_unref(ppdb_node_t* node) {
    if (!node) return;
    
    if (atomic_fetch_sub(&node->state_machine.ref_count, 1) == 1) {
        if (node->key) {
            if (node->key->flags & 1) {
                ppdb_aligned_free(node->key->extended_data);
            }
            ppdb_aligned_free(node->key);
        }
        if (node->value) {
            if (node->value->flags & 1) {
                ppdb_aligned_free(node->value->extended_data);
            }
            ppdb_aligned_free(node->value);
        }
        ppdb_aligned_free(node);
    }
}

bool node_is_active(const ppdb_node_t* node) {
    return node && !atomic_load(&node->state_machine.marked);
}

bool node_try_mark(ppdb_node_t* node) {
    if (!node) return false;
    bool expected = false;
    return atomic_compare_exchange_strong(&node->state_machine.marked, &expected, true);
}

uint32_t node_get_height(const ppdb_node_t* node) {
    return node ? node->height : 0;
}

uint32_t random_level(void) {
    uint32_t level = 1;
    while (level < MAX_SKIPLIST_LEVEL && (random() & 3) == 0) {
        level++;
    }
    return level;
}

#endif // PPDB_BASE_SKIPLIST_INC_C 