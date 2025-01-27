#include "internal/poly/poly_plugin.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/infra/infra_core.h"

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
    poly_hashtable_t* plugins;  // 插件哈希表
};

// 创建插件管理器
infra_error_t poly_plugin_mgr_create(poly_plugin_mgr_t** mgr) {
    if (!mgr) return INFRA_ERROR_INVALID_PARAM;
    
    poly_plugin_mgr_t* m = infra_malloc(sizeof(poly_plugin_mgr_t));
    if (!m) return INFRA_ERROR_NO_MEMORY;
    
    infra_error_t err = poly_hashtable_create(16, 
                                            poly_hashtable_string_hash,
                                            poly_hashtable_string_compare,
                                            &m->plugins);
    if (err != INFRA_OK) {
        infra_free(m);
        return err;
    }
    
    *mgr = m;
    return INFRA_OK;
}

// 销毁插件管理器
infra_error_t poly_plugin_mgr_destroy(poly_plugin_mgr_t* mgr) {
    if (!mgr) return INFRA_ERROR_INVALID_PARAM;
    
    // 遍历并卸载所有插件
    void plugin_unloader(const poly_hashtable_entry_t* entry, void* user_data) {
        poly_plugin_t* plugin = (poly_plugin_t*)entry->value;
        if (plugin) {
            if (plugin->handle) {
                DL_CLOSE(plugin->handle);
            }
            infra_free(plugin);
        }
    }
    
    poly_hashtable_foreach(mgr->plugins, plugin_unloader, NULL);
    poly_hashtable_destroy(mgr->plugins);
    infra_free(mgr);
    
    return INFRA_OK;
}

// 加载插件
infra_error_t poly_plugin_mgr_load(poly_plugin_mgr_t* mgr,
                                  poly_plugin_type_t type,
                                  const char* plugin_path,
                                  poly_plugin_t** plugin) {
    if (!mgr || !plugin_path || !plugin) return INFRA_ERROR_INVALID_PARAM;
    
    // 加载动态库
    DL_HANDLE handle = DL_OPEN(plugin_path);
    if (!handle) {
        return INFRA_ERROR_IO;
    }
    
    // 获取插件信息
    const char* (*get_name)(void) = DL_SYM(handle, "plugin_get_name");
    const char* (*get_version)(void) = DL_SYM(handle, "plugin_get_version");
    void* (*get_interface)(void) = DL_SYM(handle, "plugin_get_interface");
    
    if (!get_name || !get_version || !get_interface) {
        DL_CLOSE(handle);
        return INFRA_ERROR_INVALID;
    }
    
    // 创建插件对象
    poly_plugin_t* p = infra_malloc(sizeof(poly_plugin_t));
    if (!p) {
        DL_CLOSE(handle);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    p->name = get_name();
    p->version = get_version();
    p->handle = handle;
    p->interface = get_interface();
    
    // 添加到哈希表
    infra_error_t err = poly_hashtable_put(mgr->plugins, (void*)p->name, p);
    if (err != INFRA_OK) {
        DL_CLOSE(handle);
        infra_free(p);
        return err;
    }
    
    *plugin = p;
    return INFRA_OK;
}

// 卸载插件
infra_error_t poly_plugin_mgr_unload(poly_plugin_mgr_t* mgr,
                                    poly_plugin_t* plugin) {
    if (!mgr || !plugin) return INFRA_ERROR_INVALID_PARAM;
    
    // 从哈希表中移除
    infra_error_t err = poly_hashtable_remove(mgr->plugins, plugin->name);
    if (err != INFRA_OK) {
        return err;
    }
    
    // 卸载动态库
    if (plugin->handle) {
        DL_CLOSE(plugin->handle);
    }
    
    infra_free(plugin);
    return INFRA_OK;
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