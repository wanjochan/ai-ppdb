#include "internal/poly/poly_plugin.h"
#include "internal/infra/infra_core.h"

//cosmopolitan
#define DL_HANDLE void*
#define DL_OPEN(path) dlopen(path, RTLD_NOW)
#define DL_CLOSE(handle) dlclose(handle)
#define DL_SYM(handle, symbol) dlsym(handle, symbol)
#define DL_ERROR() dlerror()

#define MAX_PLUGINS 16

struct poly_plugin_mgr {
    const poly_plugin_interface_t* plugins[MAX_PLUGINS];
    size_t plugin_count;
};

static poly_plugin_mgr_t* g_plugin_mgr = NULL;

// 创建插件管理器
infra_error_t poly_plugin_mgr_create(poly_plugin_mgr_t** mgr) {
    if (!mgr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_plugin_mgr_t* new_mgr = malloc(sizeof(poly_plugin_mgr_t));
    if (!new_mgr) {
        return INFRA_ERROR_NO_MEMORY;
    }

    memset(new_mgr, 0, sizeof(poly_plugin_mgr_t));
    *mgr = new_mgr;
    return INFRA_OK;
}

// 销毁插件管理器
void poly_plugin_mgr_destroy(poly_plugin_mgr_t* mgr) {
    if (mgr) {
        free(mgr);
    }
}

// 注册内置插件
infra_error_t poly_plugin_register_builtin(poly_plugin_mgr_t* mgr, const poly_plugin_interface_t* interface) {
    if (!mgr || !interface) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mgr->plugin_count >= MAX_PLUGINS) {
        return INFRA_ERROR_NO_MEMORY;
    }

    mgr->plugins[mgr->plugin_count++] = interface;
    return INFRA_OK;
}

// 加载插件
infra_error_t poly_plugin_mgr_load(poly_plugin_mgr_t* mgr, const char* path) {
    if (!mgr || !path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO: 实现动态库加载
    return INFRA_ERROR_NOT_SUPPORTED;
}

// 获取插件
infra_error_t poly_plugin_mgr_get(poly_plugin_mgr_t** mgr) {
    if (!mgr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!g_plugin_mgr) {
        infra_error_t err = poly_plugin_mgr_create(&g_plugin_mgr);
        if (err != INFRA_OK) {
            return err;
        }
    }

    *mgr = g_plugin_mgr;
    return INFRA_OK;
}

// 获取插件接口
const poly_plugin_interface_t* poly_plugin_get_interface(poly_plugin_mgr_t* mgr, const char* name) {
    if (!mgr || !name) {
        return NULL;
    }

    for (size_t i = 0; i < mgr->plugin_count; i++) {
        if (strcmp(mgr->plugins[i]->name, name) == 0) {
            return mgr->plugins[i];
        }
    }

    return NULL;
} 
