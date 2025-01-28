#include "internal/poly/poly_duckdbkv.h"
#include "internal/poly/poly_duckdb.h"
#include "internal/poly/poly_plugin.h"

// 插件信息
const char* plugin_get_name(void) {
    return "duckdb";
}

const char* plugin_get_version(void) {
    return "1.0.0";
}

// DuckDB插件接口
typedef struct {
    infra_error_t (*init)(void** handle);
    void (*cleanup)(void* handle);
    infra_error_t (*open)(void** db, const char* path);
    void (*close)(void* db);
    infra_error_t (*get)(void* db, const char* key, void** value, size_t* value_len);
    infra_error_t (*set)(void* db, const char* key, const void* value, size_t value_len);
    infra_error_t (*del)(void* db, const char* key);
    infra_error_t (*exec)(void* db, const char* sql);
    infra_error_t (*iter_create)(void* db, void** iter);
    infra_error_t (*iter_next)(void* iter, char** key, void** value, size_t* value_len);
    void (*iter_destroy)(void* iter);
} duckdb_interface_t;

// 静态接口实例
static duckdb_interface_t g_duckdb_interface = {
    .init = poly_duckdbkv_init,
    .cleanup = poly_duckdbkv_cleanup,
    .open = poly_duckdbkv_open,
    .close = poly_duckdbkv_close,
    .get = poly_duckdbkv_get,
    .set = poly_duckdbkv_set,
    .del = poly_duckdbkv_del,
    .exec = poly_duckdbkv_exec,
    .iter_create = poly_duckdbkv_iter_create,
    .iter_next = poly_duckdbkv_iter_next,
    .iter_destroy = poly_duckdbkv_iter_destroy
};

// 获取插件接口
void* plugin_get_interface(void) {
    return &g_duckdb_interface;
} 
