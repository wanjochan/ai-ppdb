#include "ppdb/ppdb_advance.h"
#include "ppdb/ppdb.h"
#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 跳表迭代器实现
//-----------------------------------------------------------------------------

typedef struct skiplist_iterator {
    ppdb_base_t* base;
    ppdb_node_t* current;
    ppdb_key_t* end_key;
    bool include_end;
} skiplist_iterator_t;

static ppdb_error_t skiplist_iterator_next(ppdb_iterator_t* iter) {
    skiplist_iterator_t* internal = (skiplist_iterator_t*)iter->internal;
    if (!internal->current || !internal->current->next[0]) {
        return PPDB_ERR_NOT_FOUND;
    }
    
    internal->current = internal->current->next[0];
    
    // 检查是否到达结束位置
    if (internal->end_key) {
        int cmp = memcmp(internal->current->key->data, 
                        internal->end_key->data,
                        MIN(internal->current->key->size, internal->end_key->size));
        if (cmp > 0 || (cmp == 0 && !internal->include_end)) {
            return PPDB_ERR_NOT_FOUND;
        }
    }
    
    return PPDB_OK;
}

static ppdb_error_t skiplist_iterator_current(ppdb_iterator_t* iter, 
                                            ppdb_key_t* key, 
                                            ppdb_value_t* value) {
    skiplist_iterator_t* internal = (skiplist_iterator_t*)iter->internal;
    if (!internal->current) {
        return PPDB_ERR_NOT_FOUND;
    }
    
    // 复制当前节点的键值
    key->data = internal->current->key->data;
    key->size = internal->current->key->size;
    value->data = internal->current->value->data;
    value->size = internal->current->value->size;
    
    return PPDB_OK;
}

static void skiplist_iterator_destroy(ppdb_iterator_t* iter) {
    if (iter) {
        skiplist_iterator_t* internal = (skiplist_iterator_t*)iter->internal;
        if (internal) {
            free(internal);
        }
        free(iter);
    }
}

//-----------------------------------------------------------------------------
// 范围扫描实现
//-----------------------------------------------------------------------------

static ppdb_error_t skiplist_scan_impl(ppdb_base_t* base,
                                     const ppdb_scan_options_t* options,
                                     ppdb_iterator_t** iterator) {
    ppdb_error_t err;
    
    // 创建迭代器
    ppdb_iterator_t* iter = calloc(1, sizeof(ppdb_iterator_t));
    if (!iter) return PPDB_ERR_OUT_OF_MEMORY;
    
    skiplist_iterator_t* internal = calloc(1, sizeof(skiplist_iterator_t));
    if (!internal) {
        free(iter);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    
    // 初始化迭代器
    internal->base = base;
    internal->current = base->storage.head;  // 从头节点开始
    internal->end_key = options->end_key;
    internal->include_end = options->include_end;
    
    iter->internal = internal;
    iter->next = skiplist_iterator_next;
    iter->current = skiplist_iterator_current;
    iter->destroy = skiplist_iterator_destroy;
    
    // 如果有起始键，找到起始位置
    if (options->start_key) {
        ppdb_node_t* current = base->storage.head;
        int level = current->height - 1;
        
        while (level >= 0) {
            while (current->next[level] && 
                   memcmp(current->next[level]->key->data,
                         options->start_key->data,
                         MIN(current->next[level]->key->size, 
                             options->start_key->size)) < 0) {
                current = current->next[level];
            }
            level--;
        }
        
        if (options->include_start) {
            internal->current = current;
        } else {
            internal->current = current->next[0];
        }
    }
    
    *iterator = iter;
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// 性能指标实现
//-----------------------------------------------------------------------------

static ppdb_error_t metrics_get_impl(ppdb_base_t* base,
                                   ppdb_metrics_t* metrics) {
    if (!base || !metrics) return PPDB_ERR_NULL_POINTER;
    
    // 获取基础计数器
    metrics->get_count = atomic_load(&base->metrics.get_count);
    metrics->get_hits = atomic_load(&base->metrics.get_hits);
    metrics->put_count = atomic_load(&base->metrics.put_count);
    metrics->delete_count = atomic_load(&base->metrics.remove_count);
    
    // TODO: 实现延迟统计和内存统计
    metrics->avg_get_latency = 0;
    metrics->avg_put_latency = 0;
    metrics->scan_count = 0;
    metrics->memory_used = 0;
    metrics->memory_limit = 0;
    
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// 高级功能初始化
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_advance_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    
    // 分配高级操作结构
    base->advance = calloc(1, sizeof(ppdb_advance_ops_t));
    if (!base->advance) return PPDB_ERR_OUT_OF_MEMORY;
    
    // 根据存储类型设置相应的实现
    base->advance->scan = skiplist_scan_impl;
    base->advance->metrics_get = metrics_get_impl;
    
    return PPDB_OK;
}

void ppdb_advance_cleanup(ppdb_base_t* base) {
    if (base && base->advance) {
        free(base->advance);
        base->advance = NULL;
    }
}
