#ifndef _PLUGIN_H_
#define _PLUGIN_H_

/* 主程序导出的接口结构 */
typedef struct host_api {
    /* 基础设施函数 */
    int (*printf)(const char* fmt, ...);
    void* (*malloc)(size_t size);
    void (*free)(void* ptr);
    void* (*memcpy)(void* dst, const void* src, size_t n);
    void* (*memset)(void* s, int c, size_t n);
    /* 可以继续添加更多函数... */
} host_api_t;

/* 插件主函数类型 */
typedef int (*plugin_main_fn)(const host_api_t* api);

/* 插件结构 */
typedef struct plugin {
    void* base;           /* 插件基地址 */
    size_t size;         /* 插件大小 */
    plugin_main_fn main; /* 主函数指针 */
} plugin_t;

/* 加载插件 */
plugin_t* load_plugin(const char* path);

/* 卸载插件 */
void unload_plugin(plugin_t* p);

#endif /* _PLUGIN_H_ */ 