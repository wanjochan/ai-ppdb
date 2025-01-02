#include "ppdb/base.h"

// 全局操作表
static ppdb_ops_t g_ops_table[32] = {0};  // 支持最多32种类型

// 初始化函数
ppdb_error_t ppdb_init(ppdb_base_t* base, ppdb_type_t type) {
    if (!base || type <= 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    memset(base, 0, sizeof(ppdb_base_t));
    base->header.type = type;
    base->header.refs = 1;
    return PPDB_OK;
}

// 注册操作函数
ppdb_error_t ppdb_register_ops(ppdb_type_t type, const ppdb_ops_t* ops) {
    if (type <= 0 || !ops) {
        return PPDB_ERR_INVALID_ARG;
    }

    uint32_t index = __builtin_ctz(type);  // 获取最低位1的位置
    if (index >= 32) {
        return PPDB_ERR_INVALID_ARG;
    }

    g_ops_table[index] = *ops;
    return PPDB_OK;
}

// 获取操作函数
static ppdb_ops_t* ppdb_get_ops(ppdb_type_t type) {
    if (type <= 0) {
        return NULL;
    }
    uint32_t index = __builtin_ctz(type);
    if (index >= 32) {
        return NULL;
    }
    return &g_ops_table[index];
}

// 核心操作函数
ppdb_error_t ppdb_get(void* impl, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!impl || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_base_t* base = (ppdb_base_t*)impl;
    ppdb_ops_t* ops = ppdb_get_ops(base->header.type);
    if (!ops || !ops->get) {
        return PPDB_ERR_NOT_SUPPORTED;
    }

    ppdb_error_t err = ops->get(impl, key, value);
    
    // 更新统计信息
    atomic_fetch_add(&base->storage.metrics.get_count, 1);
    if (err == PPDB_OK) {
        atomic_fetch_add(&base->storage.metrics.get_hits, 1);
    } else {
        atomic_fetch_add(&base->storage.metrics.get_miss_count, 1);
    }
    
    return err;
}

ppdb_error_t ppdb_put(void* impl, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!impl || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_base_t* base = (ppdb_base_t*)impl;
    ppdb_ops_t* ops = ppdb_get_ops(base->header.type);
    if (!ops || !ops->put) {
        return PPDB_ERR_NOT_SUPPORTED;
    }

    ppdb_error_t err = ops->put(impl, key, value);
    
    // 更新统计信息
    if (err == PPDB_OK) {
        atomic_fetch_add(&base->storage.metrics.put_count, 1);
        atomic_fetch_add(&base->storage.metrics.total_bytes, key->size + value->size);
    }
    
    return err;
}

ppdb_error_t ppdb_remove(void* impl, const ppdb_key_t* key) {
    if (!impl || !key) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_base_t* base = (ppdb_base_t*)impl;
    ppdb_ops_t* ops = ppdb_get_ops(base->header.type);
    if (!ops || !ops->remove) {
        return PPDB_ERR_NOT_SUPPORTED;
    }

    ppdb_error_t err = ops->remove(impl, key);
    
    // 更新统计信息
    if (err == PPDB_OK) {
        atomic_fetch_add(&base->storage.metrics.delete_count, 1);
    }
    
    return err;
}

ppdb_error_t ppdb_clear(void* impl) {
    if (!impl) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_base_t* base = (ppdb_base_t*)impl;
    ppdb_ops_t* ops = ppdb_get_ops(base->header.type);
    if (!ops || !ops->clear) {
        return PPDB_ERR_NOT_SUPPORTED;
    }

    return ops->clear(impl);
}

// 工具函数
void* ppdb_get_extra(ppdb_node_t* node) {
    return node ? node->extra : NULL;
}

uint32_t ppdb_get_type(const ppdb_base_t* base) {
    return base ? base->header.type : 0;
}
