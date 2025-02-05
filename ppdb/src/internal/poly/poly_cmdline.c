#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include <string.h>
#include <ctype.h>

// 命令列表
static poly_cmd_t* g_commands = NULL;
static int g_command_count = 0;
static int g_command_capacity = 0;

// 全局配置
static poly_config_t g_config;

// 内部函数声明
static infra_error_t parse_service_line(char* line, poly_service_config_t* service);
static poly_service_type_t get_service_type(const char* type_str);
static void trim_string(char* str);

infra_error_t poly_cmdline_init(void)
{
    INFRA_LOG_DEBUG("Initializing command line framework");

    // 初始化命令列表
    g_command_capacity = 16;  // 初始容量
    g_commands = infra_malloc(g_command_capacity * sizeof(poly_cmd_t));
    if (!g_commands) {
        INFRA_LOG_ERROR("Failed to allocate command list");
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化全局配置
    memset(&g_config, 0, sizeof(poly_config_t));
    g_config.log_level = 3; // 默认日志级别

    INFRA_LOG_DEBUG("Command line framework initialized successfully");
    return INFRA_OK;
}

infra_error_t poly_cmdline_cleanup(void) {
    if (g_commands) {
        infra_free(g_commands);
        g_commands = NULL;
    }
    g_command_count = 0;
    g_command_capacity = 0;
    return INFRA_OK;
}

infra_error_t poly_cmdline_register(const poly_cmd_t* cmd) {
    INFRA_LOG_DEBUG("Registering command: %s", cmd ? cmd->name : "NULL");
    if (cmd == NULL) {
        INFRA_LOG_ERROR("Invalid command");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_command_count >= g_command_capacity) {
        INFRA_LOG_ERROR("Too many commands");
        return INFRA_ERROR_NO_MEMORY;
    }

    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, cmd->name) == 0) {
            INFRA_LOG_ERROR("Command %s already exists", cmd->name);
            return INFRA_ERROR_EXISTS;
        }
    }

    memcpy(&g_commands[g_command_count], cmd, sizeof(poly_cmd_t));
    g_command_count++;

    INFRA_LOG_INFO("Command %s registered", cmd->name);
    INFRA_LOG_DEBUG("Command count: %d", g_command_count);
    return INFRA_OK;
}

infra_error_t poly_cmdline_parse_config(const char* config_file, poly_config_t* config) {
    INFRA_LOG_DEBUG("Parsing config file: %s", config_file);
    
    FILE* fp = fopen(config_file, "r");
    if (!fp) {
        INFRA_LOG_ERROR("Failed to open config file: %s", config_file);
        return INFRA_ERROR_IO;
    }

    char line[POLY_CMD_MAX_VALUE];
    int line_num = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        trim_string(line);
        
        // 跳过空行和注释
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // 解析服务配置行
        if (config->service_count >= POLY_CMD_MAX_SERVICES) {
            INFRA_LOG_ERROR("Too many services defined in config file");
            fclose(fp);
            return INFRA_ERROR_NO_MEMORY;
        }

        infra_error_t err = parse_service_line(line, &config->services[config->service_count]);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to parse service config at line %d", line_num);
            fclose(fp);
            return err;
        }

        config->service_count++;
    }

    fclose(fp);
    INFRA_LOG_INFO("Successfully parsed config file with %d services", config->service_count);
    return INFRA_OK;
}

infra_error_t poly_cmdline_parse_args(int argc, char** argv, poly_config_t* config) {
    INFRA_LOG_DEBUG("Parsing command line arguments");
    
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0) {
            char* option = argv[i] + 2;
            char* value = strchr(option, '=');
            
            if (value) {
                *value = '\0';
                value++;
                
                if (strcmp(option, "config") == 0) {
                    strncpy(config->config_file, value, POLY_CMD_MAX_VALUE - 1);
                } else if (strcmp(option, "log-level") == 0) {
                    config->log_level = atoi(value);
                }
            }
        }
    }
    
    return INFRA_OK;
}

infra_error_t poly_cmdline_execute(int argc, char** argv) {
    INFRA_LOG_DEBUG("Executing command with argc=%d", argc);
    if (argc < 1 || argv == NULL) {
        INFRA_LOG_ERROR("Invalid arguments");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 解析全局参数
    poly_config_t config;
    memset(&config, 0, sizeof(poly_config_t));
    config.log_level = g_config.log_level;  // 使用默认值

    infra_error_t err = poly_cmdline_parse_args(argc, argv, &config);
    if (err != INFRA_OK) {
        return err;
    }

    // 如果指定了配置文件，解析它
    if (config.config_file[0] != '\0') {
        err = poly_cmdline_parse_config(config.config_file, &config);
        if (err != INFRA_OK) {
            return err;
        }
    }

    // 查找并执行命令
    const char* cmd_name = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            cmd_name = argv[i];
            break;
        }
    }

    if (!cmd_name) {
        INFRA_LOG_ERROR("No command specified");
        return INFRA_ERROR_INVALID_PARAM;
    }

    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, cmd_name) == 0) {
            INFRA_LOG_INFO("Executing command %s", cmd_name);
            return g_commands[i].handler(&config, argc, argv);
        }
    }

    INFRA_LOG_ERROR("Command %s not found", cmd_name);
    return INFRA_ERROR_NOT_FOUND;
}

// 内部辅助函数实现
static void trim_string(char* str) {
    char* start = str;
    char* end;
    
    // 跳过开头的空白字符
    while (isspace(*start)) start++;
    
    if (*start == 0) {
        *str = 0;
        return;
    }
    
    // 找到结尾的非空白字符
    end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    
    *(end + 1) = 0;
    
    if (start != str) {
        memmove(str, start, end - start + 2);
    }
}

static poly_service_type_t get_service_type(const char* type_str) {
    if (strcmp(type_str, "rinetd") == 0) return POLY_SERVICE_RINETD;
    if (strcmp(type_str, "sqlite") == 0) return POLY_SERVICE_SQLITE;
    if (strcmp(type_str, "memkv") == 0) return POLY_SERVICE_MEMKV;
    if (strcmp(type_str, "diskv") == 0) return POLY_SERVICE_DISKV;
    return -1;
}

static infra_error_t parse_service_line(char* line, poly_service_config_t* service) {
    char* tokens[6];
    int token_count = 0;
    char* token = strtok(line, " \t");
    
    while (token && token_count < 6) {
        tokens[token_count++] = token;
        token = strtok(NULL, " \t");
    }
    
    if (token_count < 4) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 解析监听地址和端口
    strncpy(service->listen_host, tokens[0], POLY_CMD_MAX_NAME - 1);
    service->listen_port = atoi(tokens[1]);
    
    // 解析服务类型和后端配置
    service->type = get_service_type(tokens[2]);
    if (service->type == -1) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    strncpy(service->backend, tokens[3], POLY_CMD_MAX_VALUE - 1);
    
    return INFRA_OK;
}

const poly_cmd_t* poly_cmdline_get_commands(int* count) {
    if (count) {
        *count = g_command_count;
    }
    return g_commands;
}

infra_error_t poly_cmdline_help(const char* cmd_name) {
    if (cmd_name == NULL) {
        INFRA_LOG_INFO("Usage: ppdb [--config=<file>] [--log-level=<level>] <command> [options]");
        INFRA_LOG_INFO("Available commands:");
        
        for (int i = 0; i < g_command_count; i++) {
            INFRA_LOG_INFO("  %-20s %s", g_commands[i].name, g_commands[i].desc);
        }
    } else {
        for (int i = 0; i < g_command_count; i++) {
            if (strcmp(g_commands[i].name, cmd_name) == 0) {
                INFRA_LOG_INFO("Command: %s", cmd_name);
                INFRA_LOG_INFO("Description: %s", g_commands[i].desc);
                
                if (g_commands[i].options != NULL && g_commands[i].option_count > 0) {
                    INFRA_LOG_INFO("Options:");
                    for (int j = 0; j < g_commands[i].option_count; j++) {
                        INFRA_LOG_INFO("  --%s%s\t%s",
                            g_commands[i].options[j].name,
                            g_commands[i].options[j].has_value ? "=<value>" : "",
                            g_commands[i].options[j].desc);
                    }
                }
                return INFRA_OK;
            }
        }
        INFRA_LOG_ERROR("Command %s not found", cmd_name);
        return INFRA_ERROR_NOT_FOUND;
    }
    return INFRA_OK;
} 