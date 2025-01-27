#ifndef POLY_PLUGIN_H
#define POLY_PLUGIN_H

#include "internal/infra/infra_core.h"

// 插件类型
typedef enum {
    POLY_PLUGIN_SQLITE = 1,
    POLY_PLUGIN_DUCKDB = 2,
    POLY_PLUGIN_CUSTOM = 3
} poly_plugin_type_t;

// 插件接口
typedef struct {
    const char* name;     // 插件名称
    const char* version;  // 插件版本
    void* handle;        // 动态库句柄
    void* interface;     // 插件接口
} poly_plugin_t;

// 内置插件
typedef struct {
    const char* name;
    void* interface;
    poly_plugin_type_t type;
} poly_builtin_plugin_t;

// 插件管理器
typedef struct poly_plugin_mgr poly_plugin_mgr_t;

// 创建插件管理器
infra_error_t poly_plugin_mgr_create(poly_plugin_mgr_t** mgr);

// 销毁插件管理器
void poly_plugin_mgr_destroy(poly_plugin_mgr_t* mgr);

// 注册内置插件
infra_error_t poly_plugin_register_builtin(poly_plugin_mgr_t* mgr, 
                                         const poly_builtin_plugin_t* plugin);

// 加载动态插件
infra_error_t poly_plugin_mgr_load(poly_plugin_mgr_t* mgr,
                                  poly_plugin_type_t type,
                                  const char* path,
                                  poly_plugin_t** plugin);

// 卸载插件
infra_error_t poly_plugin_mgr_unload(poly_plugin_mgr_t* mgr,
                                    poly_plugin_t* plugin);

// 获取插件接口
void* poly_plugin_get_interface(poly_plugin_t* plugin);

#endif // POLY_PLUGIN_H 