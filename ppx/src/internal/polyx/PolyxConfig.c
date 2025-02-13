#include "PolyxConfig.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

// Internal helper functions
static void trim_string(char* str) {
    if (!str) return;
    
    char* start = str;
    char* end;
    
    while (isspace(*start)) start++;
    
    if (*start == 0) {
        *str = 0;
        return;
    }
    
    end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    
    *(end + 1) = 0;
    
    if (start != str) {
        memmove(str, start, end - start + 2);
    }
}

// Configuration file parsing
infrax_error_t polyx_config_parse_file(const char* filename, polyx_config_t* config) {
    if (!filename || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        return INFRAX_ERROR_IO;
    }

    char line[MAX_LINE_LENGTH];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        // Skip empty lines and comments
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            continue;
        }

        // Remove trailing newline and spaces
        trim_string(line);
        if (line[0] == '\0') {
            continue;
        }

        // Parse service type
        char service_type[32];
        char rest_of_line[MAX_LINE_LENGTH];
        if (sscanf(line, "%31s %[^\n]", service_type, rest_of_line) != 2) {
            fclose(fp);
            return INFRAX_ERROR_INVALID_PARAM;
        }

        // Get service type
        polyx_service_type_t type;
        infrax_error_t err = polyx_config_get_service_type_by_name(service_type, &type);
        if (err != INFRAX_OK) {
            fclose(fp);
            return err;
        }

        // Parse service specific configuration
        polyx_service_config_t* svc = &config->services[config->service_count];
        svc->type = type;

        switch (type) {
            case POLYX_SERVICE_RINETD: {
                char src_addr[64], dst_addr[64];
                int src_port, dst_port;
                if (sscanf(rest_of_line, "%63s %d %63s %d", 
                          src_addr, &src_port, dst_addr, &dst_port) != 4) {
                    fclose(fp);
                    return INFRAX_ERROR_INVALID_PARAM;
                }
                strncpy(svc->listen_host, src_addr, POLYX_CMD_MAX_NAME - 1);
                svc->listen_port = src_port;
                strncpy(svc->target_host, dst_addr, POLYX_CMD_MAX_NAME - 1);
                svc->target_port = dst_port;
                break;
            }
            case POLYX_SERVICE_SQLITE:
            case POLYX_SERVICE_MEMKV: {
                char listen_addr[64], backend[256];
                int listen_port;
                if (sscanf(rest_of_line, "%63s %d %255s", 
                          listen_addr, &listen_port, backend) != 3) {
                    fclose(fp);
                    return INFRAX_ERROR_INVALID_PARAM;
                }
                strncpy(svc->listen_host, listen_addr, POLYX_CMD_MAX_NAME - 1);
                svc->listen_port = listen_port;
                strncpy(svc->backend, backend, POLYX_CMD_MAX_VALUE - 1);
                break;
            }
            default:
                fclose(fp);
                return INFRAX_ERROR_INVALID_PARAM;
        }

        config->service_count++;
        if (config->service_count >= POLYX_CMD_MAX_SERVICES) {
            fclose(fp);
            return INFRAX_ERROR_NO_MEMORY;
        }
    }

    fclose(fp);
    return INFRAX_OK;
}

// Service specific configuration parsing
infrax_error_t polyx_config_parse_rinetd(const char* filename, polyx_config_t* config) {
    if (!filename || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        return INFRAX_ERROR_IO;
    }

    char line[MAX_LINE_LENGTH];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        // Skip empty lines and comments
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            continue;
        }

        trim_string(line);
        if (line[0] == '\0') {
            continue;
        }

        // Parse rinetd forward rule
        char src_addr[64], dst_addr[64];
        int src_port, dst_port;
        
        if (sscanf(line, "%63s %d %63s %d", src_addr, &src_port, dst_addr, &dst_port) != 4) {
            fclose(fp);
            return INFRAX_ERROR_INVALID_PARAM;
        }

        if (config->service_count >= POLYX_CMD_MAX_SERVICES) {
            fclose(fp);
            return INFRAX_ERROR_NO_MEMORY;
        }

        // Add rinetd service config
        polyx_service_config_t* svc = &config->services[config->service_count];
        svc->type = POLYX_SERVICE_RINETD;
        strncpy(svc->listen_host, src_addr, POLYX_CMD_MAX_NAME - 1);
        svc->listen_port = src_port;
        strncpy(svc->target_host, dst_addr, POLYX_CMD_MAX_NAME - 1);
        svc->target_port = dst_port;
        
        config->service_count++;
    }

    fclose(fp);
    return INFRAX_OK;
}

// Configuration validation
infrax_error_t polyx_config_validate_service(const polyx_service_config_t* config) {
    if (!config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Validate common fields
    if (!config->listen_host[0] || config->listen_port <= 0 || config->listen_port > 65535) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Validate service specific fields
    switch (config->type) {
        case POLYX_SERVICE_RINETD:
            if (!config->target_host[0] || 
                config->target_port <= 0 || 
                config->target_port > 65535) {
                return INFRAX_ERROR_INVALID_PARAM;
            }
            break;

        case POLYX_SERVICE_SQLITE:
        case POLYX_SERVICE_MEMKV:
            if (!config->backend[0]) {
                return INFRAX_ERROR_INVALID_PARAM;
            }
            break;

        default:
            return INFRAX_ERROR_INVALID_PARAM;
    }

    return INFRAX_OK;
}

// Configuration file generation
infrax_error_t polyx_config_generate_file(const char* filename, const polyx_config_t* config) {
    if (!filename || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    FILE* fp = fopen(filename, "w");
    if (!fp) {
        return INFRAX_ERROR_IO;
    }

    // Write header
    fprintf(fp, "# PPX Configuration File\n");
    fprintf(fp, "# Generated by PPX\n\n");

    // Write services
    for (int i = 0; i < config->service_count; i++) {
        const polyx_service_config_t* svc = &config->services[i];
        
        // Write service type
        fprintf(fp, "%s ", polyx_config_get_service_type_name(svc->type));

        // Write service specific configuration
        switch (svc->type) {
            case POLYX_SERVICE_RINETD:
                fprintf(fp, "%s %d %s %d\n",
                    svc->listen_host, svc->listen_port,
                    svc->target_host, svc->target_port);
                break;

            case POLYX_SERVICE_SQLITE:
            case POLYX_SERVICE_MEMKV:
                fprintf(fp, "%s %d %s\n",
                    svc->listen_host, svc->listen_port,
                    svc->backend);
                break;

            default:
                fclose(fp);
                return INFRAX_ERROR_INVALID_PARAM;
        }
    }

    fclose(fp);
    return INFRAX_OK;
}

// Configuration utilities
const char* polyx_config_get_service_type_name(polyx_service_type_t type) {
    switch (type) {
        case POLYX_SERVICE_RINETD: return "rinetd";
        case POLYX_SERVICE_SQLITE: return "sqlite";
        case POLYX_SERVICE_MEMKV: return "memkv";
        case POLYX_SERVICE_DISKV: return "diskv";
        default: return "unknown";
    }
}

infrax_error_t polyx_config_get_service_type_by_name(const char* name, polyx_service_type_t* type) {
    if (!name || !type) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    if (strcmp(name, "rinetd") == 0) {
        *type = POLYX_SERVICE_RINETD;
    } else if (strcmp(name, "sqlite") == 0) {
        *type = POLYX_SERVICE_SQLITE;
    } else if (strcmp(name, "memkv") == 0) {
        *type = POLYX_SERVICE_MEMKV;
    } else if (strcmp(name, "diskv") == 0) {
        *type = POLYX_SERVICE_DISKV;
    } else {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    return INFRAX_OK;
} 