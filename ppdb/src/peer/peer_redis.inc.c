#include "peer.h"

// Redis command types
typedef enum {
    REDIS_CMD_GET,
    REDIS_CMD_SET,
    REDIS_CMD_DEL,
    REDIS_CMD_UNKNOWN
} redis_cmd_t;

// Redis command parser
typedef struct {
    redis_cmd_t type;
    char key[256];
    char* value;
    size_t value_len;
    bool ex;
    int64_t expire;
} redis_parser_t;

// Protocol instance data
typedef struct {
    redis_parser_t parser;
    char buffer[4096];
    size_t buffer_used;
    int multi_bulk_len;
    int bulk_len;
    char* current_arg;
    int arg_count;
} redis_proto_t;

// Create protocol instance
static ppdb_error_t redis_proto_create(void** proto, void* user_data) {
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
static ppdb_error_t redis_proto_on_connect(void* proto, ppdb_conn_t conn) {
    redis_proto_t* p = proto;
    p->buffer_used = 0;
    p->multi_bulk_len = 0;
    p->bulk_len = -1;
    p->current_arg = NULL;
    p->arg_count = 0;
    return PPDB_OK;
}

// Handle connection closed
static void redis_proto_on_disconnect(void* proto, ppdb_conn_t conn) {
    redis_proto_t* p = proto;
    p->buffer_used = 0;
    p->multi_bulk_len = 0;
    p->bulk_len = -1;
    p->current_arg = NULL;
    p->arg_count = 0;
}

// Parse RESP protocol
static ppdb_error_t parse_resp(redis_proto_t* p) {
    char* ptr = p->buffer;
    char* end = p->buffer + p->buffer_used;
    
    // Need more data
    if (ptr >= end) {
        return PPDB_OK;
    }
    
    // Parse multi-bulk length
    if (p->multi_bulk_len == 0) {
        if (*ptr != '*') {
            return PPDB_ERR_PROTOCOL;
        }
        ptr++;
        
        char* newline = memchr(ptr, '\r', end - ptr);
        if (!newline || newline + 1 >= end || newline[1] != '\n') {
            return PPDB_OK; // Need more data
        }
        
        *newline = '\0';
        p->multi_bulk_len = atoi(ptr);
        if (p->multi_bulk_len < 1) {
            return PPDB_ERR_PROTOCOL;
        }
        
        ptr = newline + 2;
    }
    
    // Parse bulk strings
    while (ptr < end && p->arg_count < p->multi_bulk_len) {
        // Parse bulk length
        if (p->bulk_len == -1) {
            if (*ptr != '$') {
                return PPDB_ERR_PROTOCOL;
            }
            ptr++;
            
            char* newline = memchr(ptr, '\r', end - ptr);
            if (!newline || newline + 1 >= end || newline[1] != '\n') {
                return PPDB_OK; // Need more data
            }
            
            *newline = '\0';
            p->bulk_len = atoi(ptr);
            if (p->bulk_len < 0) {
                return PPDB_ERR_PROTOCOL;
            }
            
            ptr = newline + 2;
            p->current_arg = ptr;
        }
        
        // Check if we have complete bulk string
        if (end - ptr < p->bulk_len + 2) {
            return PPDB_OK; // Need more data
        }
        
        if (ptr[p->bulk_len] != '\r' || ptr[p->bulk_len + 1] != '\n') {
            return PPDB_ERR_PROTOCOL;
        }
        
        // Store argument
        if (p->arg_count == 0) {
            // Command name
            ptr[p->bulk_len] = '\0';
            if (strcasecmp(ptr, "get") == 0) {
                p->parser.type = REDIS_CMD_GET;
            }
            else if (strcasecmp(ptr, "set") == 0) {
                p->parser.type = REDIS_CMD_SET;
            }
            else if (strcasecmp(ptr, "del") == 0) {
                p->parser.type = REDIS_CMD_DEL;
            }
            else {
                p->parser.type = REDIS_CMD_UNKNOWN;
            }
        }
        else if (p->arg_count == 1) {
            // Key
            ptr[p->bulk_len] = '\0';
            strncpy(p->parser.key, ptr, sizeof(p->parser.key) - 1);
        }
        else if (p->arg_count == 2 && p->parser.type == REDIS_CMD_SET) {
            // Value for SET
            p->parser.value = ptr;
            p->parser.value_len = p->bulk_len;
        }
        else if (p->arg_count > 2 && p->parser.type == REDIS_CMD_SET) {
            // Optional SET arguments
            if (strcasecmp(ptr, "ex") == 0) {
                p->parser.ex = true;
            }
            else if (p->parser.ex && p->arg_count == 4) {
                ptr[p->bulk_len] = '\0';
                p->parser.expire = atoll(ptr);
            }
        }
        
        ptr += p->bulk_len + 2;
        p->bulk_len = -1;
        p->current_arg = NULL;
        p->arg_count++;
    }
    
    // Command complete
    if (p->arg_count == p->multi_bulk_len) {
        // Reset parser state
        size_t consumed = ptr - p->buffer;
        if (consumed < p->buffer_used) {
            memmove(p->buffer, ptr, p->buffer_used - consumed);
            p->buffer_used -= consumed;
        }
        else {
            p->buffer_used = 0;
        }
        
        p->multi_bulk_len = 0;
        p->bulk_len = -1;
        p->current_arg = NULL;
        p->arg_count = 0;
        
        return PPDB_OK;
    }
    
    return PPDB_OK;
}

// Send redis error
static ppdb_error_t send_error(ppdb_conn_t conn, const char* msg) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "-ERR %s\r\n", msg);
    return ppdb_conn_send(conn, buf, len);
}

// Send redis simple string
static ppdb_error_t send_simple_string(ppdb_conn_t conn, const char* str) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "+%s\r\n", str);
    return ppdb_conn_send(conn, buf, len);
}

// Send redis integer
static ppdb_error_t send_integer(ppdb_conn_t conn, int64_t val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), ":%lld\r\n", val);
    return ppdb_conn_send(conn, buf, len);
}

// Send redis bulk string
static ppdb_error_t send_bulk_string(ppdb_conn_t conn, const char* str, size_t len) {
    char buf[32];
    int header_len = snprintf(buf, sizeof(buf), "$%zu\r\n", len);
    
    ppdb_error_t err = ppdb_conn_send(conn, buf, header_len);
    if (err != PPDB_OK) return err;
    
    if (len > 0) {
        err = ppdb_conn_send(conn, str, len);
        if (err != PPDB_OK) return err;
    }
    
    return ppdb_conn_send(conn, "\r\n", 2);
}

// Send redis null bulk string
static ppdb_error_t send_null(ppdb_conn_t conn) {
    return ppdb_conn_send(conn, "$-1\r\n", 5);
}

// Handle GET command
static ppdb_error_t handle_get(redis_proto_t* p, ppdb_conn_t conn) {
    // TODO: Get value from storage
    const char* value = "test_value";
    return send_bulk_string(conn, value, strlen(value));
}

// Handle SET command
static ppdb_error_t handle_set(redis_proto_t* p, ppdb_conn_t conn) {
    // TODO: Store value in storage
    return send_simple_string(conn, "OK");
}

// Handle DEL command
static ppdb_error_t handle_del(redis_proto_t* p, ppdb_conn_t conn) {
    // TODO: Delete value from storage
    return send_integer(conn, 1);
}

// Handle incoming data
static ppdb_error_t redis_proto_on_data(void* proto, ppdb_conn_t conn,
                                     const uint8_t* data, size_t size) {
    redis_proto_t* p = proto;
    
    // Append to buffer
    if (p->buffer_used + size > sizeof(p->buffer)) {
        return PPDB_ERR_BUFFER_FULL;
    }
    memcpy(p->buffer + p->buffer_used, data, size);
    p->buffer_used += size;
    
    // Parse RESP protocol
    ppdb_error_t err = parse_resp(p);
    if (err != PPDB_OK) {
        return err;
    }
    
    // Handle command if complete
    if (p->arg_count == p->multi_bulk_len && p->multi_bulk_len > 0) {
        switch (p->parser.type) {
            case REDIS_CMD_GET:
                err = handle_get(p, conn);
                break;
                
            case REDIS_CMD_SET:
                err = handle_set(p, conn);
                break;
                
            case REDIS_CMD_DEL:
                err = handle_del(p, conn);
                break;
                
            default:
                err = send_error(conn, "unknown command");
                break;
        }
    }
    
    return err;
}

// Get protocol name
static const char* redis_proto_get_name(void* proto) {
    return "redis";
}

// Protocol operations
const peer_ops_t peer_redis_ops = {
    .create = redis_proto_create,
    .destroy = redis_proto_destroy,
    .on_connect = redis_proto_on_connect,
    .on_disconnect = redis_proto_on_disconnect,
    .on_data = redis_proto_on_data,
    .get_name = redis_proto_get_name
}; 