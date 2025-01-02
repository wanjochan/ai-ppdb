#include "ppdb/base.h"

// 全局操作表
static ppdb_ops_t g_ops_table[32] = {0};  // 支持最多32种类型

// 初始化函数
ppdb_status_t ppdb_init(ppdb_base_t* base, ppdb_type_t type) {
    if (!base || type <= 0) {
        return PPDB_INVALID_ARGUMENT;
    }

    memset(base, 0, sizeof(ppdb_base_t));
    base->header.type = type;
    base->header.refs = 1;
    return PPDB_OK;
}

// 注册操作函数
ppdb_status_t ppdb_register_ops(ppdb_type_t type, const ppdb_ops_t* ops) {
    if (type <= 0 || !ops) {
        return PPDB_INVALID_ARGUMENT;
    }

    uint32_t index = __builtin_ctz(type);  // 获取最低位1的位置
    if (index >= 32) {
        return PPDB_INVALID_ARGUMENT;
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
ppdb_status_t ppdb_get(void* impl, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!impl || !key || !value) {
        return PPDB_INVALID_ARGUMENT;
    }

    ppdb_base_t* base = (ppdb_base_t*)impl;
    ppdb_ops_t* ops = ppdb_get_ops(base->header.type);
    if (!ops || !ops->get) {
        return PPDB_NOT_SUPPORTED;
    }

    return ops->get(impl, key, value);
}

ppdb_status_t ppdb_put(void* impl, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!impl || !key || !value) {
        return PPDB_INVALID_ARGUMENT;
    }

    ppdb_base_t* base = (ppdb_base_t*)impl;
    ppdb_ops_t* ops = ppdb_get_ops(base->header.type);
    if (!ops || !ops->put) {
        return PPDB_NOT_SUPPORTED;
    }

    return ops->put(impl, key, value);
}

ppdb_status_t ppdb_remove(void* impl, const ppdb_key_t* key) {
    if (!impl || !key) {
        return PPDB_INVALID_ARGUMENT;
    }

    ppdb_base_t* base = (ppdb_base_t*)impl;
    ppdb_ops_t* ops = ppdb_get_ops(base->header.type);
    if (!ops || !ops->remove) {
        return PPDB_NOT_SUPPORTED;
    }

    return ops->remove(impl, key);
}

ppdb_status_t ppdb_clear(void* impl) {
    if (!impl) {
        return PPDB_INVALID_ARGUMENT;
    }

    ppdb_base_t* base = (ppdb_base_t*)impl;
    ppdb_ops_t* ops = ppdb_get_ops(base->header.type);
    if (!ops || !ops->clear) {
        return PPDB_NOT_SUPPORTED;
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

void ppdb_stats_update(ppdb_stats_t* stats, ppdb_status_t status) {
    if (!stats) {
        return;
    }

    if (status == PPDB_OK) {
        stats->get_hits++;
    } else {
        stats->get_misses++;
    }
}
