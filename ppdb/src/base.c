#include "ppdb/ppdb.h"

// 类型检查函数
ppdb_error_t ppdb_check_type(ppdb_base_t* base, ppdb_type_t type) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    return (base->header.type & type) ? PPDB_OK : PPDB_ERR_INVALID_TYPE;
}

// 统一的创建接口
ppdb_error_t ppdb_create(ppdb_type_t type, ppdb_base_t** base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    
    *base = calloc(1, sizeof(ppdb_base_t));
    if (!*base) return PPDB_ERR_OUT_OF_MEMORY;
    
    (*base)->header.type = type;
    atomic_init(&(*base)->header.refs, 1);
    
    // 根据类型初始化
    switch (type) {
        case PPDB_TYPE_SKIPLIST:
            return skiplist_init(*base);
        case PPDB_TYPE_MEMTABLE:
            return memtable_init(*base);
        case PPDB_TYPE_SHARDED:
            return sharded_init(*base);
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

// 统一的销毁接口
void ppdb_destroy(ppdb_base_t* base) {
    if (!base) return;
    
    if (atomic_fetch_sub(&base->header.refs, 1) == 1) {
        switch (base->header.type) {
            case PPDB_TYPE_SKIPLIST:
                skiplist_destroy(base);
                break;
            case PPDB_TYPE_MEMTABLE:
                memtable_destroy(base);
                break;
            case PPDB_TYPE_SHARDED:
                sharded_destroy(base);
                break;
        }
        free(base);
    }
}

// 统一的操作分发
ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    atomic_fetch_add(&base->metrics.get_count, 1);
    
    ppdb_error_t err;
    switch (base->header.type) {
        case PPDB_TYPE_SKIPLIST:
            err = skiplist_get(base, key, value);
            break;
        case PPDB_TYPE_MEMTABLE:
            err = memtable_get(base, key, value);
            break;
        case PPDB_TYPE_SHARDED:
            err = sharded_get(base, key, value);
            break;
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    if (err == PPDB_OK) {
        atomic_fetch_add(&base->metrics.get_hits, 1);
    }
    return err;
}

ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    atomic_fetch_add(&base->metrics.put_count, 1);
    
    switch (base->header.type) {
        case PPDB_TYPE_SKIPLIST:
            return skiplist_put(base, key, value);
        case PPDB_TYPE_MEMTABLE:
            return memtable_put(base, key, value);
        case PPDB_TYPE_SHARDED:
            return sharded_put(base, key, value);
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    if (!base || !key) return PPDB_ERR_NULL_POINTER;
    
    atomic_fetch_add(&base->metrics.remove_count, 1);
    
    switch (base->header.type) {
        case PPDB_TYPE_SKIPLIST:
            return skiplist_remove(base, key);
        case PPDB_TYPE_MEMTABLE:
            return memtable_remove(base, key);
        case PPDB_TYPE_SHARDED:
            return sharded_remove(base, key);
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

// 引用计数管理
void ppdb_ref(ppdb_base_t* base) {
    if (base) {
        atomic_fetch_add(&base->header.refs, 1);
    }
}

void ppdb_unref(ppdb_base_t* base) {
    if (base && atomic_fetch_sub(&base->header.refs, 1) == 1) {
        ppdb_destroy(base);
    }
}
