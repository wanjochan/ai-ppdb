#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* 插件版本和魔数 */
#define PLUGIN_VERSION 1
#define PLUGIN_MAGIC 0x50504442 /* "PPDB" */

/* 插件函数代码 */
static const unsigned char test6_func_code[] = {
    0x55,                   /* push   %rbp */
    0x48, 0x89, 0xe5,      /* mov    %rsp,%rbp */
    0xb8, 0x2a, 0x00, 0x00, 0x00,  /* mov    $0x2a,%eax */
    0x5d,                   /* pop    %rbp */
    0xc3                    /* ret */
};

/* 插件函数表结构 */
#pragma pack(push, 1)
struct plugin_interface {
    uint32_t magic;    /* 魔数，用于识别插件 */
    uint32_t version;  /* 版本号 */
    unsigned char code[16];  /* 函数代码 */
};
#pragma pack(pop)

/* 导出插件接口 */
__attribute__((section(".plugin")))
struct plugin_interface plugin_api = {
    .magic = PLUGIN_MAGIC,
    .version = PLUGIN_VERSION,
    .code = {
        0x55,                   /* push   %rbp */
        0x48, 0x89, 0xe5,      /* mov    %rsp,%rbp */
        0xb8, 0x2a, 0x00, 0x00, 0x00,  /* mov    $0x2a,%eax */
        0x5d,                   /* pop    %rbp */
        0xc3                    /* ret */
    }
}; 