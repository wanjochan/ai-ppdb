#ifndef PEER_MEMKV_H
#define PEER_MEMKV_H

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_net.h"

// Service interface functions
infra_error_t memkv_init(void);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
infra_error_t memkv_cmd_handler(const char* cmd, char* response, size_t size);

// Get memkv service instance
peer_service_t* peer_memkv_get_service(void);

#endif /* PEER_MEMKV_H */