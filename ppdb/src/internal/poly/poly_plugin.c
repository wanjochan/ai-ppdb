#include "internal/poly/poly_plugin.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/infra/infra_core.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define DL_HANDLE HMODULE
#define DL_OPEN(path) LoadLibraryA(path)
#define DL_CLOSE(handle) FreeLibrary(handle)
#define DL_SYM(handle, symbol) GetProcAddress(handle, symbol)
#define DL_ERROR() GetLastError()
#else
#include <dlfcn.h>
#define DL_HANDLE void*
#define DL_OPEN(path) dlopen(path, RTLD_NOW)
#define DL_CLOSE(handle) dlclose(handle)
#define DL_SYM(handle, symbol) dlsym(handle, symbol)
#define DL_ERROR() dlerror()
#endif

// 插件管理器结构
struct poly_plugin_mgr {
    poly_plugin_t* plugins[16];  // 最多支持16个插件
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
        poly_plugin_mgr_unload(mgr, mgr->plugins[i]);
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
    plugin->version = "builtin";
    plugin->handle = NULL;  // 内置插件无需handle
    plugin->interface = builtin->interface;

    // 添加到管理器
    mgr->plugins[mgr->plugin_count++] = plugin;
    
    return INFRA_OK;
}

// 加载动态插件
infra_error_t poly_plugin_mgr_load(poly_plugin_mgr_t* mgr,
                                  poly_plugin_type_t type,
                                  const char* path,
                                  poly_plugin_t** plugin) {
    if (mgr == NULL || path == NULL) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mgr->plugin_count >= 16) {
        return INFRA_ERROR_NO_SPACE;
    }

    // 加载动态库
#ifdef _WIN32
    HMODULE handle = LoadLibraryA(path);
#else
    void* handle = dlopen(path, RTLD_LAZY);
#endif
    if (handle == NULL) {
        return INFRA_ERROR_IO;
    }

    // 获取插件信息函数
#ifdef _WIN32
    const char* (*get_name)() = (const char* (*)())GetProcAddress(handle, "plugin_get_name");
    const char* (*get_version)() = (const char* (*)())GetProcAddress(handle, "plugin_get_version");
    void* (*get_interface)() = (void* (*)())GetProcAddress(handle, "plugin_get_interface");
#else
    const char* (*get_name)() = (const char* (*)())dlsym(handle, "plugin_get_name");
    const char* (*get_version)() = (const char* (*)())dlsym(handle, "plugin_get_version");
    void* (*get_interface)() = (void* (*)())dlsym(handle, "plugin_get_interface");
#endif

    if (get_name == NULL || get_version == NULL || get_interface == NULL) {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return INFRA_ERROR_INVALID_FORMAT;
    }

    // 创建插件对象
    poly_plugin_t* new_plugin = (poly_plugin_t*)infra_malloc(sizeof(poly_plugin_t));
    if (new_plugin == NULL) {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化插件
    new_plugin->name = get_name();
    new_plugin->version = get_version();
    new_plugin->handle = handle;
    new_plugin->interface = get_interface();

    // 添加到管理器
    mgr->plugins[mgr->plugin_count++] = new_plugin;
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
        if (mgr->plugins[i] == plugin) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // 如果是动态插件，卸载动态库
    if (plugin->handle != NULL) {
#ifdef _WIN32
        FreeLibrary((HMODULE)plugin->handle);
#else
        dlclose(plugin->handle);
#endif
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

// 获取插件接口
void* poly_plugin_get_interface(poly_plugin_t* plugin) {
    return plugin ? plugin->interface : NULL;
}

// 获取插件
infra_error_t poly_plugin_mgr_get(poly_plugin_mgr_t* mgr,
                                 const char* name,
                                 poly_plugin_t** plugin) {
    if (!mgr || !name || !plugin) return INFRA_ERROR_INVALID_PARAM;
    
    void* value = NULL;
    infra_error_t err = poly_hashtable_get(mgr->plugins, name, &value);
    if (err != INFRA_OK) {
        return err;
    }
    
    *plugin = (poly_plugin_t*)value;
    return INFRA_OK;
} 