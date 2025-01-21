#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* 插件版本和魔数 */
#define PLUGIN_VERSION 1
#define PLUGIN_MAGIC 0x50504442 /* "PPDB" */

/* 错误码定义 */
#define ERR_SUCCESS 0
#define ERR_INVALID_PARAM -1
#define ERR_OUT_OF_MEMORY -2
#define ERR_NETWORK_ERROR -3

/* 基础数据类型 */
typedef struct {
    uint32_t size;
    uint8_t* data;
} buffer_t;

/* 插件函数表结构 */
#pragma pack(push, 1)
struct plugin_interface {
    uint32_t magic;    /* 魔数，用于识别插件 */
    uint32_t version;  /* 版本号 */
    
    /* Core模块函数 */
    unsigned char core_init[16];    /* 初始化core模块 */
    unsigned char core_alloc[16];   /* 内存分配函数 */
    
    /* Net模块函数 */
    unsigned char net_connect[16];  /* 网络连接函数 */
    unsigned char net_send[16];     /* 数据发送函数 */
};
#pragma pack(pop)

/* 导出插件接口 */
__attribute__((section(".plugin")))
__attribute__((used))
struct plugin_interface plugin_api = {
    .magic = PLUGIN_MAGIC,
    .version = PLUGIN_VERSION,
    
    /* Core模块函数 */
    .core_init = {
        0x55,                   /* push   %rbp */
        0x48, 0x89, 0xe5,      /* mov    %rsp,%rbp */
        0x31, 0xc0,            /* xor    %eax,%eax */
        0x5d,                   /* pop    %rbp */
        0xc3                    /* ret */
    },
    .core_alloc = {
        0x55,                   /* push   %rbp */
        0x48, 0x89, 0xe5,      /* mov    %rsp,%rbp */
        0x48, 0x89, 0xf8,      /* mov    %rdi,%rax */
        0x48, 0x83, 0xc0, 0x10, /* add    $0x10,%rax */
        0x5d,                   /* pop    %rbp */
        0xc3                    /* ret */
    },
    
    /* Net模块函数 */
    .net_connect = {
        0x55,                   /* push   %rbp */
        0x48, 0x89, 0xe5,      /* mov    %rsp,%rbp */
        0xb8, 0x2a, 0x00, 0x00, 0x00,  /* mov    $0x2a,%eax */
        0x5d,                   /* pop    %rbp */
        0xc3                    /* ret */
    },
    .net_send = {
        0x55,                   /* push   %rbp */
        0x48, 0x89, 0xe5,      /* mov    %rsp,%rbp */
        0x48, 0x89, 0xf8,      /* mov    %rdi,%rax */
        0x5d,                   /* pop    %rbp */
        0xc3                    /* ret */
    }
}; 