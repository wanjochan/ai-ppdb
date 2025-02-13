#ifndef PPX_ARCH_H
#define PPX_ARCH_H

#include "internal/arch/PpxInfra.h"

//这个文件是给未来插件、脚本做为架构来调用的

// 统一架构入口
typedef struct {
    PpxInfra* infra;      // 基础设施
    void* poly;                 // 配置系统
    void* peer;                 // 服务系统
} PpxArch;

// 全局单例
PpxArch* ppx_arch(void);

// 初始化配置系统
infra_error_t ppx_arch_init_poly(const char* config_path);

// 初始化服务系统
infra_error_t ppx_arch_init_peer(void);

// 执行命令
infra_error_t ppx_arch_exec(const char* cmd, char* response, size_t size);

#endif // PPX_ARCH_H
