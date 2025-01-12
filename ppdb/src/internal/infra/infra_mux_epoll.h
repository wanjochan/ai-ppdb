/*
 * infra_mux_epoll.h - EPOLL Multiplexing Implementation
 */

#ifndef INFRA_MUX_EPOLL_H
#define INFRA_MUX_EPOLL_H

#include "infra_mux.h"

// 创建EPOLL多路复用实例
infra_error_t infra_mux_epoll_create(const infra_config_t* config, infra_mux_t** mux);

#endif /* INFRA_MUX_EPOLL_H */ 