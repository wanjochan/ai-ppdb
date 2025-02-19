#ifndef POLYX_CONFIG_H
#define POLYX_CONFIG_H

#include "PolyxCmdline.h"
#include "internal/infrax/InfraxCore.h"

// Configuration file parsing
InfraxError polyx_config_parse_file(const char* filename, polyx_config_t* config);

// Parse service specific configuration
InfraxError polyx_config_parse_rinetd(const char* filename, polyx_config_t* config);
InfraxError polyx_config_parse_sqlite(const char* filename, polyx_config_t* config);
InfraxError polyx_config_parse_memkv(const char* filename, polyx_config_t* config);

// Service configuration validation
InfraxError polyx_config_validate_service(const polyx_service_config_t* config);

// Configuration file generation
InfraxError polyx_config_generate_file(const char* filename, const polyx_config_t* config);

// Configuration utilities
const char* polyx_config_get_service_type_name(polyx_service_type_t type);
InfraxError polyx_config_get_service_type_by_name(const char* name, polyx_service_type_t* type);

#endif // POLYX_CONFIG_H 