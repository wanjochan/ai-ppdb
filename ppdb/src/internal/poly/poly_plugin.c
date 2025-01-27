#include "internal/poly/poly_plugin.h"
#include "internal/infra/infra_core.h"

//cosmopolitan
#define DL_HANDLE void*
#define DL_OPEN(path) dlopen(path, RTLD_NOW)
#define DL_CLOSE(handle) dlclose(handle)
#define DL_SYM(handle, symbol) dlsym(handle, symbol)
#define DL_ERROR() dlerror()


// 插件管理器结构
struct poly_plugin_mgr {
    struct {
        poly_plugin_t* plugin;
        void* handle;  // 动态库句柄
    } plugins[16];  // 最多支持16个插件
    int plugin_count;
};

// 创建插件管理器
infra_error_t poly_plugin_mgr_create(poly_plugin_mgr_t** mgr) {
    *mgr = (poly_plugin_mgr_t*)infra_malloc(sizeof(poly_plugin_mgr_t));
    if (*mgr == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    memset(*mgr, 0, sizeof(poly_plugin_mgr_t));
    return INFRA_OK;
}

// 销毁插件管理器
void poly_plugin_mgr_destroy(poly_plugin_mgr_t* mgr) {
    if (mgr == NULL) return;

    // 卸载所有插件
    for (int i = 0; i < mgr->plugin_count; i++) {
        poly_plugin_mgr_unload(mgr, mgr->plugins[i].plugin);
    }

    infra_free(mgr);
}

// 注册内置插件
infra_error_t poly_plugin_register_builtin(poly_plugin_mgr_t* mgr,
    const poly_builtin_plugin_t* builtin) {
    if (mgr == NULL || builtin == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mgr->plugin_count >= 16) {
        return INFRA_ERROR_NO_SPACE;
    }

    // 创建插件对象
    poly_plugin_t* plugin = (poly_plugin_t*)infra_malloc(sizeof(poly_plugin_t));
    if (plugin == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化插件
    plugin->name = builtin->name;
    plugin->type = builtin->type;
    plugin->interface = builtin->interface;

    // 添加到管理器
    mgr->plugins[mgr->plugin_count].plugin = plugin;
    mgr->plugins[mgr->plugin_count].handle = NULL;  // 内置插件无需handle
    mgr->plugin_count++;
    
    return INFRA_OK;
}

// 加载插件
infra_error_t poly_plugin_mgr_load(poly_plugin_mgr_t* mgr,
    poly_plugin_type_t type, const char* path, poly_plugin_t** plugin) {
    if (mgr == NULL || path == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mgr->plugin_count >= 16) {
        return INFRA_ERROR_NO_SPACE;
    }

    // 加载动态库
    void* handle = dlopen(path, RTLD_LAZY);
    if (handle == NULL) {
        return INFRA_ERROR_IO;
    }

    // 获取插件信息函数
    const char* (*get_name)() = (const char* (*)())dlsym(handle, "plugin_get_name");
    void* (*get_interface)() = (void* (*)())dlsym(handle, "plugin_get_interface");

    if (get_name == NULL || get_interface == NULL) {
        dlclose(handle);
        return INFRA_ERROR_INVALID_FORMAT;
    }

    // 创建插件对象
    poly_plugin_t* new_plugin = (poly_plugin_t*)infra_malloc(sizeof(poly_plugin_t));
    if (new_plugin == NULL) {
        dlclose(handle);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化插件
    new_plugin->name = get_name();
    new_plugin->type = type;
    new_plugin->interface = get_interface();

    // 添加到管理器
    mgr->plugins[mgr->plugin_count].plugin = new_plugin;
    mgr->plugins[mgr->plugin_count].handle = handle;
    mgr->plugin_count++;
    *plugin = new_plugin;

    return INFRA_OK;
}

// 卸载插件
infra_error_t poly_plugin_mgr_unload(poly_plugin_mgr_t* mgr,
    poly_plugin_t* plugin) {
    if (mgr == NULL || plugin == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 查找插件
    int index = -1;
    for (int i = 0; i < mgr->plugin_count; i++) {
        if (mgr->plugins[i].plugin == plugin) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // 如果是动态插件，卸载动态库
    if (mgr->plugins[index].handle != NULL) {
        dlclose(mgr->plugins[index].handle);
    }

    // 释放插件对象
    infra_free(plugin);

    // 从管理器中移除
    for (int i = index; i < mgr->plugin_count - 1; i++) {
        mgr->plugins[i] = mgr->plugins[i + 1];
    }
    mgr->plugin_count--;

    return INFRA_OK;
}

// 获取插件
infra_error_t poly_plugin_mgr_get(poly_plugin_mgr_t* mgr, 
    poly_plugin_type_t type, const char* name, poly_plugin_t** plugin) {
    if (!mgr || !name || !plugin) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 在插件列表中查找
    for (int i = 0; i < mgr->plugin_count; i++) {
        if (mgr->plugins[i].plugin->type == type && 
            strcmp(mgr->plugins[i].plugin->name, name) == 0) {
            *plugin = mgr->plugins[i].plugin;
            return INFRA_OK;
        }
    }

    return INFRA_ERROR_NOT_FOUND;
}

// 获取插件接口
const poly_plugin_interface_t* poly_plugin_get_interface(const poly_plugin_t* plugin) {
    return plugin ? plugin->interface : NULL;
} 
