#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Service Registry Implementation
//-----------------------------------------------------------------------------

// 服务注册表
static struct {
    peer_service_t* services[SERVICE_TYPE_COUNT];
} g_registry = {0};

// 注册服务
infra_error_t peer_service_register(peer_service_t* service) {
    if (!service) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (service->config.type <= SERVICE_TYPE_UNKNOWN || 
        service->config.type >= SERVICE_TYPE_COUNT) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_registry.services[service->config.type]) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    g_registry.services[service->config.type] = service;
    INFRA_LOG_INFO("Registered service: %s", service->config.name);

    return INFRA_OK;
}

// 获取服务
peer_service_t* peer_service_get_by_type(peer_service_type_t type) {
    if (type <= SERVICE_TYPE_UNKNOWN || type >= SERVICE_TYPE_COUNT) {
        return NULL;
    }
    return g_registry.services[type];
}

peer_service_t* peer_service_get(const char* name) {
    if (!name) {
        return NULL;
    }

    for (int i = SERVICE_TYPE_UNKNOWN + 1; i < SERVICE_TYPE_COUNT; i++) {
        peer_service_t* service = g_registry.services[i];
        if (service && strcmp(service->config.name, name) == 0) {
            return service;
        }
    }

    return NULL;
}

// 获取服务名称
const char* peer_service_get_name(peer_service_type_t type) {
    peer_service_t* service = peer_service_get_by_type(type);
    return service ? service->config.name : NULL;
}

// 获取服务状态
peer_service_state_t peer_service_get_state(peer_service_type_t type) {
    peer_service_t* service = peer_service_get_by_type(type);
    return service ? service->state : SERVICE_STATE_STOPPED;
} 