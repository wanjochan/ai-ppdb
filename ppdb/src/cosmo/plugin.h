#ifndef _PLUGIN_H_
#define _PLUGIN_H_

/* 插件主函数类型 */
typedef int (*plugin_main_fn)(void);

/* 插件结构 */
typedef struct {
    void* base;           /* 基地址 */
    size_t size;         /* 大小 */
    plugin_main_fn main; /* 主函数 */
} plugin_t;

/* 加载插件 */
plugin_t* load_plugin(const char* path);

/* 卸载插件 */
void unload_plugin(plugin_t* p);

#endif /* _PLUGIN_H_ */ 