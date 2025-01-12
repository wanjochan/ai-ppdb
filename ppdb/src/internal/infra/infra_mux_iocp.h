/*
 * infra_mux_iocp.h - IOCP Multiplexing Implementation
 */

#ifndef INFRA_MUX_IOCP_H
#define INFRA_MUX_IOCP_H

#include "infra_mux.h"

// 创建IOCP多路复用实例
infra_error_t infra_mux_iocp_create(const infra_config_t* config, infra_mux_t** mux);

#endif /* INFRA_MUX_IOCP_H */ 