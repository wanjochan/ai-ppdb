#include "PolyxConfig.h"
#include "internal/infrax/InfraxCore.h"

#define MAX_LINE_LENGTH 1024

// Get core singleton
static InfraxCore* g_core = NULL;

// Helper function to read a line from file
static InfraxError read_line(InfraxHandle handle, char* line, size_t max_len) {
    if (!line || max_len == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    size_t pos = 0;
    char ch;
    size_t bytes_read;

    while (pos < max_len - 1) {
        InfraxError err = g_core->file_read(g_core, handle, &ch, 1, &bytes_read);
        if (!INFRAX_ERROR_IS_OK(err)) {
            return err;
        }
        if (bytes_read == 0) { // EOF
            break;
        }

        if (ch == '\n') {
            break;
        }
        line[pos++] = ch;
    }

    line[pos] = '\0';
    return make_error(INFRAX_ERROR_OK, NULL);
}

// Helper function to parse space-separated values
static InfraxError parse_values(const char* line, char* values[], size_t max_values, size_t* num_values) {
    if (!line || !values || !num_values) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    *num_values = 0;
    char* str = (char*)line;
    
    while (*str && *num_values < max_values) {
        // Skip leading spaces
        while (g_core->isspace(g_core, *str)) str++;
        if (!*str) break;

        // Find end of value
        char* start = str;
        while (*str && !g_core->isspace(g_core, *str)) str++;
        
        // Save value
        size_t len = str - start;
        values[*num_values] = (char*)g_core->malloc(g_core, len + 1);
        if (!values[*num_values]) {
            return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate memory");
        }
        g_core->memcpy(g_core, values[*num_values], start, len);
        values[*num_values][len] = '\0';
        (*num_values)++;
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Internal helper functions
static void trim_string(char* str) {
    if (!str) return;
    
    if (!g_core) {
        g_core = InfraxCoreClass.singleton();
    }
    
    char* start = str;
    char* end;
    
    while (g_core->isspace(g_core, *start)) start++;
    
    if (*start == 0) {
        *str = 0;
        return;
    }
    
    end = start + g_core->strlen(g_core, start) - 1;
    while (end > start && g_core->isspace(g_core, *end)) end--;
    
    *(end + 1) = 0;
    
    if (start != str) {
        g_core->memmove(g_core, str, start, end - start + 2);
    }
}

// Configuration file parsing
InfraxError polyx_config_parse_file(const char* filename, polyx_config_t* config) {
    if (!filename || !config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    if (!g_core) {
        g_core = InfraxCoreClass.singleton();
    }

    InfraxHandle handle;
    InfraxError err = g_core->file_open(g_core, filename, INFRAX_FILE_RDONLY, 0644, &handle);
    if (!INFRAX_ERROR_IS_OK(err)) {
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Failed to open file");
    }

    char line[MAX_LINE_LENGTH];
    int line_num = 0;

    while (INFRAX_ERROR_IS_OK(read_line(handle, line, sizeof(line)))) {
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

        // Parse service type and rest of line
        char* values[5] = {NULL}; // Max 5 values per line
        size_t num_values = 0;
        err = parse_values(line, values, 5, &num_values);
        if (!INFRAX_ERROR_IS_OK(err) || num_values < 2) {
            g_core->file_close(g_core, handle);
            return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid line format");
        }

        // Get service type
        polyx_service_type_t type;
        err = polyx_config_get_service_type_by_name(values[0], &type);
        if (!INFRAX_ERROR_IS_OK(err)) {
            g_core->file_close(g_core, handle);
            return err;
        }

        // Parse service specific configuration
        polyx_service_config_t* svc = &config->services[config->service_count];
        svc->type = type;

        switch (type) {
            case POLYX_SERVICE_RINETD: {
                if (num_values != 5) {
                    g_core->file_close(g_core, handle);
                    return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid rinetd format");
                }
                g_core->strncpy(g_core, svc->listen_host, values[1], POLYX_CMD_MAX_NAME - 1);
                svc->listen_port = g_core->atoi(g_core, values[2]);
                g_core->strncpy(g_core, svc->target_host, values[3], POLYX_CMD_MAX_NAME - 1);
                svc->target_port = g_core->atoi(g_core, values[4]);
                break;
            }
            case POLYX_SERVICE_SQLITE:
            case POLYX_SERVICE_MEMKV: {
                if (num_values != 4) {
                    g_core->file_close(g_core, handle);
                    return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid service format");
                }
                g_core->strncpy(g_core, svc->listen_host, values[1], POLYX_CMD_MAX_NAME - 1);
                svc->listen_port = g_core->atoi(g_core, values[2]);
                g_core->strncpy(g_core, svc->backend, values[3], POLYX_CMD_MAX_VALUE - 1);
                break;
            }
            default:
                g_core->file_close(g_core, handle);
                return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid service type");
        }

        // Free allocated values
        for (size_t i = 0; i < num_values; i++) {
            if (values[i]) g_core->free(g_core, values[i]);
        }

        config->service_count++;
        if (config->service_count >= POLYX_CMD_MAX_SERVICES) {
            g_core->file_close(g_core, handle);
            return make_error(INFRAX_ERROR_NO_MEMORY, "Too many services");
        }
    }

    g_core->file_close(g_core, handle);
    return make_error(INFRAX_ERROR_OK, NULL);
}

// Service specific configuration parsing
InfraxError polyx_config_parse_rinetd(const char* filename, polyx_config_t* config) {
    if (!filename || !config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    if (!g_core) {
        g_core = InfraxCoreClass.singleton();
    }

    InfraxHandle handle;
    InfraxError err = g_core->file_open(g_core, filename, INFRAX_FILE_RDONLY, 0644, &handle);
    if (!INFRAX_ERROR_IS_OK(err)) {
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Failed to open file");
    }

    char line[MAX_LINE_LENGTH];
    int line_num = 0;

    while (INFRAX_ERROR_IS_OK(read_line(handle, line, sizeof(line)))) {
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
        char* values[4] = {NULL}; // src_addr, src_port, dst_addr, dst_port
        size_t num_values = 0;
        err = parse_values(line, values, 4, &num_values);
        if (!INFRAX_ERROR_IS_OK(err) || num_values != 4) {
            g_core->file_close(g_core, handle);
            return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid rinetd format");
        }

        if (config->service_count >= POLYX_CMD_MAX_SERVICES) {
            g_core->file_close(g_core, handle);
            return make_error(INFRAX_ERROR_NO_MEMORY, "Too many services");
        }

        // Add rinetd service config
        polyx_service_config_t* svc = &config->services[config->service_count];
        svc->type = POLYX_SERVICE_RINETD;
        g_core->strncpy(g_core, svc->listen_host, values[0], POLYX_CMD_MAX_NAME - 1);
        svc->listen_port = g_core->atoi(g_core, values[1]);
        g_core->strncpy(g_core, svc->target_host, values[2], POLYX_CMD_MAX_NAME - 1);
        svc->target_port = g_core->atoi(g_core, values[3]);

        // Free allocated values
        for (size_t i = 0; i < num_values; i++) {
            if (values[i]) g_core->free(g_core, values[i]);
        }
        
        config->service_count++;
    }

    g_core->file_close(g_core, handle);
    return make_error(INFRAX_ERROR_OK, NULL);
}

// Configuration validation
InfraxError polyx_config_validate_service(const polyx_service_config_t* config) {
    if (!config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Validate common fields
    if (!config->listen_host[0] || config->listen_port <= 0 || config->listen_port > 65535) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid listen address/port");
    }

    // Validate service specific fields
    switch (config->type) {
        case POLYX_SERVICE_RINETD:
            if (!config->target_host[0] || 
                config->target_port <= 0 || 
                config->target_port > 65535) {
                return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid target address/port");
            }
            break;

        case POLYX_SERVICE_SQLITE:
        case POLYX_SERVICE_MEMKV:
            if (!config->backend[0]) {
                return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid backend");
            }
            break;

        default:
            return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid service type");
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Configuration file generation
InfraxError polyx_config_generate_file(const char* filename, const polyx_config_t* config) {
    if (!filename || !config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    if (!g_core) {
        g_core = InfraxCoreClass.singleton();
    }

    InfraxHandle handle;
    InfraxError err = g_core->file_open(g_core, filename, 
        INFRAX_FILE_CREATE | INFRAX_FILE_WRONLY | INFRAX_FILE_TRUNC, 0644, &handle);
    if (!INFRAX_ERROR_IS_OK(err)) {
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Failed to create file");
    }

    // Write header
    char buffer[MAX_LINE_LENGTH];
    size_t bytes_written;

    const char* header = "# PPX Configuration File\n# Generated by PPX\n\n";
    err = g_core->file_write(g_core, handle, header, g_core->strlen(g_core, header), &bytes_written);
    if (!INFRAX_ERROR_IS_OK(err)) {
        g_core->file_close(g_core, handle);
        return make_error(INFRAX_ERROR_WRITE_FAILED, "Failed to write header");
    }

    // Write services
    for (int i = 0; i < config->service_count; i++) {
        const polyx_service_config_t* svc = &config->services[i];
        const char* type_name = polyx_config_get_service_type_name(svc->type);
        
        switch (svc->type) {
            case POLYX_SERVICE_RINETD:
                g_core->snprintf(g_core, buffer, sizeof(buffer), "%s %s %d %s %d\n",
                    type_name, svc->listen_host, svc->listen_port,
                    svc->target_host, svc->target_port);
                break;

            case POLYX_SERVICE_SQLITE:
            case POLYX_SERVICE_MEMKV:
                g_core->snprintf(g_core, buffer, sizeof(buffer), "%s %s %d %s\n",
                    type_name, svc->listen_host, svc->listen_port, svc->backend);
                break;

            default:
                g_core->file_close(g_core, handle);
                return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid service type");
        }

        err = g_core->file_write(g_core, handle, buffer, g_core->strlen(g_core, buffer), &bytes_written);
        if (!INFRAX_ERROR_IS_OK(err)) {
            g_core->file_close(g_core, handle);
            return make_error(INFRAX_ERROR_WRITE_FAILED, "Failed to write service configuration");
        }
    }

    g_core->file_close(g_core, handle);
    return make_error(INFRAX_ERROR_OK, NULL);
}

// Service type name utilities
const char* polyx_config_get_service_type_name(polyx_service_type_t type) {
    switch (type) {
        case POLYX_SERVICE_RINETD:
            return "rinetd";
        case POLYX_SERVICE_SQLITE:
            return "sqlite";
        case POLYX_SERVICE_MEMKV:
            return "memkv";
        default:
            return "unknown";
    }
}

InfraxError polyx_config_get_service_type_by_name(const char* name, polyx_service_type_t* type) {
    if (!name || !type) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    if (!g_core) {
        g_core = InfraxCoreClass.singleton();
    }

    if (g_core->strcmp(g_core, name, "rinetd") == 0) {
        *type = POLYX_SERVICE_RINETD;
    } else if (g_core->strcmp(g_core, name, "sqlite") == 0) {
        *type = POLYX_SERVICE_SQLITE;
    } else if (g_core->strcmp(g_core, name, "memkv") == 0) {
        *type = POLYX_SERVICE_MEMKV;
    } else {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Unknown service type");
    }

    return make_error(INFRAX_ERROR_OK, NULL);
} 