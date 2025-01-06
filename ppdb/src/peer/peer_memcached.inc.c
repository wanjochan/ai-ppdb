#include "internal/peer.h"

// Memcached command types
typedef enum {
    MEMCACHED_CMD_GET,
    MEMCACHED_CMD_SET,
    MEMCACHED_CMD_DELETE,
    MEMCACHED_CMD_UNKNOWN
} memcached_cmd_t;

// Memcached command parser
typedef struct {
    memcached_cmd_t type;
    char key[256];
    uint32_t flags;
    uint32_t exptime;
    uint32_t bytes;
    char noreply;
    char* value;
} memcached_parser_t;

// Protocol instance data
typedef struct {
    memcached_parser_t parser;
    char buffer[4096];
    size_t buffer_used;
} memcached_proto_t;

// Create protocol instance
static ppdb_error_t memcached_proto_create(void** proto, void* user_data) {
    memcached_proto_t* p = calloc(1, sizeof(memcached_proto_t));
    if (!p) {
        return PPDB_ERR_MEMORY;
    }
    *proto = p;
    return PPDB_OK;
}

// Destroy protocol instance
static void memcached_proto_destroy(void* proto) {
    free(proto);
}

// Handle connection established
static ppdb_error_t memcached_proto_on_connect(void* proto, ppdb_conn_t conn) {
    memcached_proto_t* p = proto;
    p->buffer_used = 0;
    return PPDB_OK;
}

// Handle connection closed
static void memcached_proto_on_disconnect(void* proto, ppdb_conn_t conn) {
    memcached_proto_t* p = proto;
    p->buffer_used = 0;
}

// Parse command line
static ppdb_error_t parse_command(memcached_proto_t* p, char* line) {
    char* cmd = strtok(line, " ");
    if (!cmd) {
        return PPDB_ERR_PROTOCOL;
    }

    if (strcmp(cmd, "get") == 0) {
        p->parser.type = MEMCACHED_CMD_GET;
        char* key = strtok(NULL, " ");
        if (!key) {
            return PPDB_ERR_PROTOCOL;
        }
        strncpy(p->parser.key, key, sizeof(p->parser.key) - 1);
    }
    else if (strcmp(cmd, "set") == 0) {
        p->parser.type = MEMCACHED_CMD_SET;
        // Parse: <key> <flags> <exptime> <bytes> [noreply]
        char* key = strtok(NULL, " ");
        char* flags = strtok(NULL, " ");
        char* exptime = strtok(NULL, " ");
        char* bytes = strtok(NULL, " ");
        char* noreply = strtok(NULL, " ");
        
        if (!key || !flags || !exptime || !bytes) {
            return PPDB_ERR_PROTOCOL;
        }
        
        strncpy(p->parser.key, key, sizeof(p->parser.key) - 1);
        p->parser.flags = atoi(flags);
        p->parser.exptime = atoi(exptime);
        p->parser.bytes = atoi(bytes);
        p->parser.noreply = noreply ? 1 : 0;
    }
    else if (strcmp(cmd, "delete") == 0) {
        p->parser.type = MEMCACHED_CMD_DELETE;
        char* key = strtok(NULL, " ");
        char* noreply = strtok(NULL, " ");
        
        if (!key) {
            return PPDB_ERR_PROTOCOL;
        }
        
        strncpy(p->parser.key, key, sizeof(p->parser.key) - 1);
        p->parser.noreply = noreply ? 1 : 0;
    }
    else {
        p->parser.type = MEMCACHED_CMD_UNKNOWN;
    }

    return PPDB_OK;
}

// Handle GET command
static ppdb_error_t handle_get(memcached_proto_t* p, ppdb_conn_t conn) {
    ppdb_data_t key = {
        .data = (uint8_t*)p->parser.key,
        .size = strlen(p->parser.key)
    };
    
    ppdb_data_t value;
    ppdb_error_t err = ppdb_storage_get(conn->storage, &key, &value);
    
    if (err == PPDB_OK) {
        // VALUE <key> <flags> <bytes>\r\n
        char header[1024];
        snprintf(header, sizeof(header), "VALUE %s %u %u\r\n",
                p->parser.key, p->parser.flags, (uint32_t)value.size);
        ppdb_conn_write(conn, (uint8_t*)header, strlen(header));
        
        // <data>\r\n
        ppdb_conn_write(conn, value.data, value.size);
        ppdb_conn_write(conn, (uint8_t*)"\r\n", 2);
        
        // END\r\n
        ppdb_conn_write(conn, (uint8_t*)"END\r\n", 5);
        return PPDB_OK;
    } else if (err == PPDB_ERR_NOT_FOUND) {
        ppdb_conn_write(conn, (uint8_t*)"END\r\n", 5);
        return PPDB_OK;
    }
    
    return err;
}

// Handle SET command
static ppdb_error_t handle_set(memcached_proto_t* p, ppdb_conn_t conn) {
    if (!p->parser.value || p->parser.bytes == 0) {
        return PPDB_ERR_PROTOCOL;
    }
    
    ppdb_data_t key = {
        .data = (uint8_t*)p->parser.key,
        .size = strlen(p->parser.key)
    };
    
    ppdb_data_t value = {
        .data = (uint8_t*)p->parser.value,
        .size = p->parser.bytes
    };
    
    ppdb_error_t err = ppdb_storage_put(conn->storage, &key, &value);
    
    if (!p->parser.noreply) {
        if (err == PPDB_OK) {
            ppdb_conn_write(conn, (uint8_t*)"STORED\r\n", 8);
        } else {
            ppdb_conn_write(conn, (uint8_t*)"NOT_STORED\r\n", 12);
        }
    }
    
    return err;
}

// Handle DELETE command
static ppdb_error_t handle_delete(memcached_proto_t* p, ppdb_conn_t conn) {
    ppdb_data_t key = {
        .data = (uint8_t*)p->parser.key,
        .size = strlen(p->parser.key)
    };
    
    ppdb_error_t err = ppdb_storage_delete(conn->storage, &key);
    
    if (!p->parser.noreply) {
        if (err == PPDB_OK) {
            ppdb_conn_write(conn, (uint8_t*)"DELETED\r\n", 9);
        } else if (err == PPDB_ERR_NOT_FOUND) {
            ppdb_conn_write(conn, (uint8_t*)"NOT_FOUND\r\n", 11);
        } else {
            ppdb_conn_write(conn, (uint8_t*)"ERROR\r\n", 7);
        }
    }
    
    return err;
}

// Handle incoming data
static ppdb_error_t memcached_proto_on_data(void* proto, ppdb_conn_t conn,
                                          const uint8_t* data, size_t size) {
    memcached_proto_t* p = proto;
    
    // Append to buffer
    if (p->buffer_used + size > sizeof(p->buffer)) {
        return PPDB_ERR_BUFFER_FULL;
    }
    memcpy(p->buffer + p->buffer_used, data, size);
    p->buffer_used += size;
    
    // Find command line
    char* newline = memchr(p->buffer, '\n', p->buffer_used);
    if (!newline) {
        return PPDB_OK; // Need more data
    }
    
    // Parse command
    *newline = '\0';
    ppdb_error_t err = parse_command(p, p->buffer);
    if (err != PPDB_OK) {
        return err;
    }
    
    // Handle command
    switch (p->parser.type) {
        case MEMCACHED_CMD_GET:
            err = handle_get(p, conn);
            break;
            
        case MEMCACHED_CMD_SET:
            err = handle_set(p, conn);
            break;
            
        case MEMCACHED_CMD_DELETE:
            err = handle_delete(p, conn);
            break;
            
        default:
            err = PPDB_ERR_PROTOCOL;
            break;
    }
    
    // Reset buffer
    size_t remaining = p->buffer_used - (newline - p->buffer + 1);
    if (remaining > 0) {
        memmove(p->buffer, newline + 1, remaining);
    }
    p->buffer_used = remaining;
    
    return err;
}

// Get protocol name
static const char* memcached_proto_get_name(void* proto) {
    return "memcached";
}

// Protocol operations
const peer_ops_t peer_memcached_ops = {
    .create = memcached_proto_create,
    .destroy = memcached_proto_destroy,
    .on_connect = memcached_proto_on_connect,
    .on_disconnect = memcached_proto_on_disconnect,
    .on_data = memcached_proto_on_data,
    .get_name = memcached_proto_get_name
}; 