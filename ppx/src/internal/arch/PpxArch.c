#include "PpxArch.h"

static PpxArch g_arch = {0};
static bool g_initialized = false;

PpxArch* ppx_arch(void) {
    if (!g_initialized) {
        // 初始化基础设施
        g_arch.infra = ppx_infra();
        g_initialized = true;
    }
    return &g_arch;
}

infra_error_t ppx_arch_init_poly(const char* config_path) {
    if (!config_path) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    const PpxArch* arch = ppx_arch();
    // TODO: 初始化配置系统
    return INFRA_OK;
}

infra_error_t ppx_arch_init_peer(void) {
    const PpxArch* arch = ppx_arch();
    // TODO: 初始化服务系统
    return INFRA_OK;
}

infra_error_t ppx_arch_exec(const char* cmd, char* response, size_t size) {
    if (!cmd || !response || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    const PpxArch* arch = ppx_arch();
    // TODO: 执行命令
    return INFRA_OK;
}
