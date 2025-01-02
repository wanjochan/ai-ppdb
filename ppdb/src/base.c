#include "ppdb/base.h"

// 基础类型
struct ppdb_base {
    ppdb_storage_header_t header;
    ppdb_metrics_t metrics;
    ppdb_storage_config_t config;
    ppdb_ops_t* ops;
    ppdb_array_t array;
    atomic_size_t refs;
};

ppdb_error_t ppdb_init(ppdb_base_t* base, const ppdb_storage_config_t* config) {
    if (!base || !config) {
        return PPDB_ERR_NULL_POINTER;
    }

    memset(base, 0, sizeof(ppdb_base_t));
    memcpy(&base->config, config, sizeof(ppdb_storage_config_t));
    atomic_init(&base->refs, 1);

    return PPDB_OK;
}

ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) {
        return PPDB_ERR_NULL_POINTER;
    }

    atomic_fetch_add(&base->metrics.get_count, 1);

    if (base->ops && base->ops->get) {
        ppdb_error_t err = base->ops->get(base, key, value);
        if (err == PPDB_OK) {
            atomic_fetch_add(&base->metrics.get_hits, 1);
        } else {
            atomic_fetch_add(&base->metrics.get_miss_count, 1);
        }
        return err;
    }

    return PPDB_ERR_NOT_SUPPORTED;
}

ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) {
        return PPDB_ERR_NULL_POINTER;
    }

    atomic_fetch_add(&base->metrics.put_count, 1);
    atomic_fetch_add(&base->metrics.total_bytes, key->size + value->size);
    atomic_fetch_add(&base->metrics.total_values, 1);

    if (base->ops && base->ops->put) {
        return base->ops->put(base, key, value);
    }

    return PPDB_ERR_NOT_SUPPORTED;
}

ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    if (!base || !key) {
        return PPDB_ERR_NULL_POINTER;
    }

    atomic_fetch_add(&base->metrics.delete_count, 1);

    if (base->ops && base->ops->remove) {
        return base->ops->remove(base, key);
    }

    return PPDB_ERR_NOT_SUPPORTED;
}

ppdb_error_t ppdb_clear(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_NULL_POINTER;
    }

    if (base->ops && base->ops->clear) {
        return base->ops->clear(base);
    }

    return PPDB_ERR_NOT_SUPPORTED;
}

void ppdb_destroy(ppdb_base_t* base) {
    if (base) {
        if (atomic_fetch_sub(&base->refs, 1) == 1) {
            if (base->ops && base->ops->clear) {
                base->ops->clear(base);
            }
            if (base->array.items) {
                free(base->array.items);
            }
            memset(base, 0, sizeof(ppdb_base_t));
        }
    }
}

uint32_t ppdb_get_type(const ppdb_base_t* base) {
    return base ? base->header.type : PPDB_TYPE_NONE;
}

void ppdb_ref(ppdb_base_t* base) {
    if (base) {
        atomic_fetch_add(&base->refs, 1);
    }
}

void ppdb_unref(ppdb_base_t* base) {
    if (base) {
        if (atomic_fetch_sub(&base->refs, 1) == 1) {
            ppdb_destroy(base);
        }
    }
}
