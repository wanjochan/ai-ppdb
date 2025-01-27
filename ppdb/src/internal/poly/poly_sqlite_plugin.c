#include "internal/poly/poly_sqlite.h"
#include "internal/poly/poly_plugin.h"

// 插件信息
const char* plugin_get_name(void) {
    return "sqlite";
}

const char* plugin_get_version(void) {
    return "1.0.0";
}

// SQLite插件接口
typedef struct {
    infra_error_t (*init)(void);
    infra_error_t (*cleanup)(void);
    infra_error_t (*open)(const char* path, poly_sqlite_db_t** db);
    infra_error_t (*close)(poly_sqlite_db_t* db);
    infra_error_t (*get)(poly_sqlite_db_t* db, const void* key, size_t klen, void** val, size_t* vlen);
    infra_error_t (*put)(poly_sqlite_db_t* db, const void* key, size_t klen, const void* val, size_t vlen);
    infra_error_t (*del)(poly_sqlite_db_t* db, const void* key, size_t klen);
    infra_error_t (*begin)(poly_sqlite_db_t* db);
    infra_error_t (*commit)(poly_sqlite_db_t* db);
    infra_error_t (*rollback)(poly_sqlite_db_t* db);
    infra_error_t (*iter_create)(poly_sqlite_db_t* db, poly_sqlite_iter_t** iter);
    infra_error_t (*iter_next)(poly_sqlite_iter_t* iter, void** key, size_t* klen, void** val, size_t* vlen);
    infra_error_t (*iter_destroy)(poly_sqlite_iter_t* iter);
} sqlite_interface_t;

// 静态接口实例
static sqlite_interface_t g_sqlite_interface = {
    .init = poly_sqlite_init,
    .cleanup = poly_sqlite_cleanup,
    .open = poly_sqlite_open,
    .close = poly_sqlite_close,
    .get = poly_sqlite_get,
    .put = poly_sqlite_put,
    .del = poly_sqlite_del,
    .begin = poly_sqlite_begin,
    .commit = poly_sqlite_commit,
    .rollback = poly_sqlite_rollback,
    .iter_create = poly_sqlite_iter_create,
    .iter_next = poly_sqlite_iter_next,
    .iter_destroy = poly_sqlite_iter_destroy
};

// 获取插件接口
void* plugin_get_interface(void) {
    return &g_sqlite_interface;
} 