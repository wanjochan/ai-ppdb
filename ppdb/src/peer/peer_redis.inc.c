#ifndef PPDB_PEER_REDIS_INC_C_
#define PPDB_PEER_REDIS_INC_C_

#include <cosmopolitan.h>
#include "../internal/peer.h"
#include "../internal/storage.h"

// Buffer size for responses
#define REDIS_RESPONSE_BUFFER_SIZE 1024

// Redis command types
typedef enum {
    REDIS_CMD_GET,
    REDIS_CMD_SET,
    REDIS_CMD_DEL,
    REDIS_CMD_UNKNOWN
} redis_cmd_type_t;

// Redis protocol parser
typedef struct {
    redis_cmd_type_t type;
    char* key;
    size_t key_size;
    char* value;
    size_t value_size;
    uint32_t flags;
    uint32_t exptime;
} redis_parser_t;

// Redis protocol handler
typedef struct {
    redis_parser_t parser;
    char buffer[REDIS_RESPONSE_BUFFER_SIZE];
    size_t buffer_size;
} redis_proto_t;

// Forward declarations
static ppdb_error_t redis_handle_get(redis_proto_t* p, ppdb_handle_t conn);
static ppdb_error_t redis_handle_set(redis_proto_t* p, ppdb_handle_t conn);
static ppdb_error_t redis_handle_del(redis_proto_t* p, ppdb_handle_t conn);
static ppdb_error_t redis_parse_command(redis_proto_t* p, char* line);

// Protocol operations
static ppdb_error_t redis_send_error(ppdb_handle_t conn, const char* msg) {
    char buf[REDIS_RESPONSE_BUFFER_SIZE];
    int len = snprintf(buf, sizeof(buf), "-ERR %s\r\n", msg);
    return ppdb_conn_send(conn, buf, len);
}

static ppdb_error_t redis_send_simple_string(ppdb_handle_t conn, const char* str) {
    char buf[REDIS_RESPONSE_BUFFER_SIZE];
    int len = snprintf(buf, sizeof(buf), "+%s\r\n", str);
    return ppdb_conn_send(conn, buf, len);
}

static ppdb_error_t redis_send_integer(ppdb_handle_t conn, int64_t val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), ":%ld\r\n", val);
    return ppdb_conn_send(conn, buf, len);
}

static ppdb_error_t redis_send_bulk_string(ppdb_handle_t conn, const char* str, size_t len) {
    char header[32];
    int header_len = snprintf(header, sizeof(header), "$%zu\r\n", len);
    ppdb_error_t err = ppdb_conn_send(conn, header, header_len);
    if (err != PPDB_OK) return err;
    
    if (len > 0) {
        err = ppdb_conn_send(conn, str, len);
        if (err != PPDB_OK) return err;
    }
    
    return ppdb_conn_send(conn, "\r\n", 2);
}

static ppdb_error_t redis_send_null(ppdb_handle_t conn) {
    return ppdb_conn_send(conn, "$-1\r\n", 5);
}

// Create protocol instance
static ppdb_error_t redis_proto_create(void** proto, void* user_data) {
    PPDB_UNUSED(user_data);
    redis_proto_t* p = calloc(1, sizeof(redis_proto_t));
    if (!p) {
        return PPDB_ERR_MEMORY;
    }
    *proto = p;
    return PPDB_OK;
}

// Destroy protocol instance
static void redis_proto_destroy(void* proto) {
    free(proto);
}

// Handle connection established
static ppdb_error_t redis_proto_on_connect(void* proto, ppdb_handle_t conn) {
    PPDB_UNUSED(conn);
    redis_proto_t* p = proto;
    p->buffer_size = 0;
    return PPDB_OK;
}

// Handle connection closed
static void redis_proto_on_disconnect(void* proto, ppdb_handle_t conn) {
    PPDB_UNUSED(conn);
    redis_proto_t* p = proto;
    p->buffer_size = 0;
}

// Parse command line
static ppdb_error_t redis_parse_command(redis_proto_t* p, char* line) {
    char* cmd = strtok(line, " ");
    if (!cmd) {
        return PPDB_ERR_PROTOCOL;
    }

    if (strcmp(cmd, "GET") == 0) {
        p->parser.type = REDIS_CMD_GET;
        char* key = strtok(NULL, " ");
        if (!key) {
            return PPDB_ERR_PROTOCOL;
        }
        p->parser.key = key;
        p->parser.key_size = strlen(key);
    }
    else if (strcmp(cmd, "SET") == 0) {
        p->parser.type = REDIS_CMD_SET;
        char* key = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        
        if (!key || !value) {
            return PPDB_ERR_PROTOCOL;
        }
        
        p->parser.key = key;
        p->parser.key_size = strlen(key);
        p->parser.value = value;
        p->parser.value_size = strlen(value);
    }
    else if (strcmp(cmd, "DEL") == 0) {
        p->parser.type = REDIS_CMD_DEL;
        char* key = strtok(NULL, " ");
        
        if (!key) {
            return PPDB_ERR_PROTOCOL;
        }
        
        p->parser.key = key;
        p->parser.key_size = strlen(key);
    }
    else {
        p->parser.type = REDIS_CMD_UNKNOWN;
        return PPDB_ERR_PROTOCOL;
    }
    
    return PPDB_OK;
}

// Handle GET command
static ppdb_error_t redis_handle_get(redis_proto_t* p, ppdb_handle_t conn) {
    ppdb_peer_connection_t* connection = (ppdb_peer_connection_t*)(uintptr_t)conn;
    size_t value_size = 0;
    char value_buffer[REDIS_RESPONSE_BUFFER_SIZE];
    
    // Get value from storage
    ppdb_error_t err = ppdb_storage_get(connection->storage, p->parser.key, p->parser.key_size,
                                      value_buffer, &value_size);
    if (err != PPDB_OK) {
        if (err == PPDB_ERR_NOT_FOUND) {
            return redis_send_null(conn);
        }
        return err;
    }

    return redis_send_bulk_string(conn, value_buffer, value_size);
}

// Handle SET command
static ppdb_error_t redis_handle_set(redis_proto_t* p, ppdb_handle_t conn) {
    ppdb_peer_connection_t* connection = (ppdb_peer_connection_t*)(uintptr_t)conn;
    
    // Store value in storage
    ppdb_error_t err = ppdb_storage_put(connection->storage, p->parser.key, p->parser.key_size,
                                      p->parser.value, p->parser.value_size);
    if (err != PPDB_OK) {
        return err;
    }

    // Send response
    return redis_send_simple_string(conn, "OK");
}

// Handle DEL command
static ppdb_error_t redis_handle_del(redis_proto_t* p, ppdb_handle_t conn) {
    ppdb_peer_connection_t* connection = (ppdb_peer_connection_t*)(uintptr_t)conn;
    
    // Delete value from storage
    ppdb_error_t err = ppdb_storage_delete(connection->storage, p->parser.key, p->parser.key_size);
    if (err != PPDB_OK) {
        if (err == PPDB_ERR_NOT_FOUND) {
            return redis_send_integer(conn, 0);
        }
        return err;
    }

    return redis_send_integer(conn, 1);
}

// Handle incoming data
static ppdb_error_t redis_proto_on_data(void* proto, ppdb_handle_t conn,
                                     const uint8_t* data, size_t size) {
    redis_proto_t* p = proto;
    
    // Append to buffer
    if (p->buffer_size + size > sizeof(p->buffer)) {
        return PPDB_ERR_BUFFER_FULL;
    }
    memcpy(p->buffer + p->buffer_size, data, size);
    p->buffer_size += size;
    
    // Find command line
    char* newline = memchr(p->buffer, '\n', p->buffer_size);
    if (!newline) {
        return PPDB_OK; // Need more data
    }
    
    // Parse command
    *newline = '\0';
    ppdb_error_t err = redis_parse_command(p, p->buffer);
    if (err != PPDB_OK) {
        return err;
    }
    
    // Handle command
    switch (p->parser.type) {
        case REDIS_CMD_GET:
            err = redis_handle_get(p, conn);
            break;
            
        case REDIS_CMD_SET:
            err = redis_handle_set(p, conn);
            break;
            
        case REDIS_CMD_DEL:
            err = redis_handle_del(p, conn);
            break;
            
        default:
            err = PPDB_ERR_PROTOCOL;
            break;
    }
    
    // Reset buffer
    size_t remaining = p->buffer_size - (newline - p->buffer + 1);
    if (remaining > 0) {
        memmove(p->buffer, newline + 1, remaining);
    }
    p->buffer_size = remaining;
    
    return err;
}

// Get protocol name
static const char* redis_proto_get_name(void* proto) {
    PPDB_UNUSED(proto);
    return "redis";
}

// Protocol operations table
static const peer_ops_t peer_redis_ops = {
    .create = redis_proto_create,
    .destroy = redis_proto_destroy,
    .on_connect = redis_proto_on_connect,
    .on_disconnect = redis_proto_on_disconnect,
    .on_data = redis_proto_on_data,
    .get_name = redis_proto_get_name
};

// Protocol adapter getter
const peer_ops_t* peer_get_redis(void) {
    return &peer_redis_ops;
}

#endif // PPDB_PEER_REDIS_INC_C_ 