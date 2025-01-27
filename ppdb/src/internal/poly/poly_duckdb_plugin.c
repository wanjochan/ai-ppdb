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
    infra_error_t (*init)(void);
    infra_error_t (*cleanup)(void);
    infra_error_t (*open)(const char* path, poly_duckdb_db_t** db);
    infra_error_t (*close)(poly_duckdb_db_t* db);
    infra_error_t (*get)(poly_duckdb_db_t* db, const void* key, size_t klen, void** val, size_t* vlen);
    infra_error_t (*put)(poly_duckdb_db_t* db, const void* key, size_t klen, const void* val, size_t vlen);
    infra_error_t (*del)(poly_duckdb_db_t* db, const void* key, size_t klen);
    infra_error_t (*begin)(poly_duckdb_db_t* db);
    infra_error_t (*commit)(poly_duckdb_db_t* db);
    infra_error_t (*rollback)(poly_duckdb_db_t* db);
    infra_error_t (*iter_create)(poly_duckdb_db_t* db, poly_duckdb_iter_t** iter);
    infra_error_t (*iter_next)(poly_duckdb_iter_t* iter, void** key, size_t* klen, void** val, size_t* vlen);
    infra_error_t (*iter_destroy)(poly_duckdb_iter_t* iter);
} duckdb_interface_t;

// 静态接口实例
static duckdb_interface_t g_duckdb_interface = {
    .init = poly_duckdb_init,
    .cleanup = poly_duckdb_cleanup,
    .open = poly_duckdb_open,
    .close = poly_duckdb_close,
    .get = poly_duckdb_get,
    .put = poly_duckdb_put,
    .del = poly_duckdb_del,
    .begin = poly_duckdb_begin,
    .commit = poly_duckdb_commit,
    .rollback = poly_duckdb_rollback,
    .iter_create = poly_duckdb_iter_create,
    .iter_next = poly_duckdb_iter_next,
    .iter_destroy = poly_duckdb_iter_destroy
};

// 获取插件接口
void* plugin_get_interface(void) {
    return &g_duckdb_interface;
} 