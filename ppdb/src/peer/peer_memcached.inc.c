#ifndef PPDB_PEER_MEMCACHED_INC_C_
#define PPDB_PEER_MEMCACHED_INC_C_

#include <cosmopolitan.h>
#include "../internal/peer.h"
#include "../internal/storage.h"

// Buffer size for responses
#define MEMCACHED_RESPONSE_BUFFER_SIZE 1024

// Memcached command types
typedef enum {
    MEMCACHED_CMD_GET,
    MEMCACHED_CMD_SET,
    MEMCACHED_CMD_DELETE,
    MEMCACHED_CMD_UNKNOWN
} memcached_cmd_type_t;

// Memcached protocol parser
typedef struct {
    memcached_cmd_type_t type;
    char* key;
    size_t key_size;
    char* value;
    size_t value_size;
    uint32_t flags;
    uint32_t exptime;
    bool noreply;
} memcached_parser_t;

// Memcached protocol handler
typedef struct {
    memcached_parser_t parser;
    char buffer[MEMCACHED_RESPONSE_BUFFER_SIZE];
    size_t buffer_size;
} memcached_proto_t;

// Forward declarations
static ppdb_error_t memcached_handle_get(memcached_proto_t* p, ppdb_handle_t conn);
static ppdb_error_t memcached_handle_set(memcached_proto_t* p, ppdb_handle_t conn);
static ppdb_error_t memcached_handle_delete(memcached_proto_t* p, ppdb_handle_t conn);
static ppdb_error_t memcached_parse_command(memcached_proto_t* p, char* line);

// Protocol operations
static ppdb_error_t memcached_send_error(ppdb_handle_t conn, const char* msg) {
    char buf[MEMCACHED_RESPONSE_BUFFER_SIZE];
    int len = snprintf(buf, sizeof(buf), "ERROR %s\r\n", msg);
    return ppdb_conn_send(conn, buf, len);
}

static ppdb_error_t memcached_send_stored(ppdb_handle_t conn) {
    return ppdb_conn_send(conn, "STORED\r\n", 8);
}

static ppdb_error_t memcached_send_not_stored(ppdb_handle_t conn) {
    return ppdb_conn_send(conn, "NOT_STORED\r\n", 12);
}

static ppdb_error_t memcached_send_deleted(ppdb_handle_t conn) {
    return ppdb_conn_send(conn, "DELETED\r\n", 9);
}

static ppdb_error_t memcached_send_not_found(ppdb_handle_t conn) {
    return ppdb_conn_send(conn, "NOT_FOUND\r\n", 11);
}

static ppdb_error_t memcached_send_value(ppdb_handle_t conn, const char* key, size_t key_size,
                                     const void* value, size_t value_size, uint32_t flags) {
    char header[MEMCACHED_RESPONSE_BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header), "VALUE %.*s %u %zu\r\n",
                            (int)key_size, key, flags, value_size);
    
    ppdb_error_t err = ppdb_conn_send(conn, header, header_len);
    if (err != PPDB_OK) return err;
    
    err = ppdb_conn_send(conn, value, value_size);
    if (err != PPDB_OK) return err;
    
    return ppdb_conn_send(conn, "\r\nEND\r\n", 7);
}

// Create protocol instance
static ppdb_error_t memcached_proto_create(void** proto, void* user_data) {
    PPDB_UNUSED(user_data);
    memcached_proto_t* p = calloc(1, sizeof(memcached_proto_t));
    if (!p) {
        return PPDB_ERR_MEMORY;
    }
    *proto = p;
    return PPDB_OK;
}

// Destroy protocol instance
static void memcached_proto_destroy(void* proto) {
    PPDB_UNUSED(proto);
    free(proto);
}

// Handle connection established
static ppdb_error_t memcached_proto_on_connect(void* proto, ppdb_handle_t conn) {
    PPDB_UNUSED(conn);
    memcached_proto_t* p = proto;
    p->buffer_size = 0;
    return PPDB_OK;
}

// Handle connection closed
static void memcached_proto_on_disconnect(void* proto, ppdb_handle_t conn) {
    PPDB_UNUSED(conn);
    memcached_proto_t* p = proto;
    p->buffer_size = 0;
}

// Parse command line
static ppdb_error_t memcached_parse_command(memcached_proto_t* p, char* line) {
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
        p->parser.key = key;
        p->parser.key_size = strlen(key);
    }
    else if (strcmp(cmd, "set") == 0) {
        p->parser.type = MEMCACHED_CMD_SET;
        char* key = strtok(NULL, " ");
        char* flags = strtok(NULL, " ");
        char* exptime = strtok(NULL, " ");
        char* bytes = strtok(NULL, " ");
        char* noreply = strtok(NULL, " ");
        
        if (!key || !flags || !exptime || !bytes) {
            return PPDB_ERR_PROTOCOL;
        }
        
        p->parser.key = key;
        p->parser.key_size = strlen(key);
        p->parser.flags = atoi(flags);
        p->parser.exptime = atoi(exptime);
        p->parser.value_size = atoi(bytes);
        p->parser.noreply = noreply ? true : false;
    }
    else if (strcmp(cmd, "delete") == 0) {
        p->parser.type = MEMCACHED_CMD_DELETE;
        char* key = strtok(NULL, " ");
        char* noreply = strtok(NULL, " ");
        
        if (!key) {
            return PPDB_ERR_PROTOCOL;
        }
        
        p->parser.key = key;
        p->parser.key_size = strlen(key);
        p->parser.noreply = noreply ? true : false;
    }
    else {
        p->parser.type = MEMCACHED_CMD_UNKNOWN;
        return PPDB_ERR_PROTOCOL;
    }
    
    return PPDB_OK;
}

// Handle GET command
static ppdb_error_t memcached_handle_get(memcached_proto_t* p, ppdb_handle_t conn) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    size_t value_size = 0;
    char value_buffer[MEMCACHED_RESPONSE_BUFFER_SIZE];
    
    // Get value from storage
    ppdb_error_t err = ppdb_storage_get(state->storage, p->parser.key, p->parser.key_size,
                                      value_buffer, &value_size);
    if (err != PPDB_OK) {
        if (err == PPDB_ERR_NOT_FOUND) {
            return ppdb_conn_send(conn, "END\r\n", 5);
        }
        return err;
    }

    return memcached_send_value(conn, p->parser.key, p->parser.key_size, value_buffer, value_size, p->parser.flags);
}

// Handle SET command
static ppdb_error_t memcached_handle_set(memcached_proto_t* p, ppdb_handle_t conn) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    
    // Store value in storage
    ppdb_error_t err = ppdb_storage_put(state->storage, p->parser.key, p->parser.key_size,
                                      p->parser.value, p->parser.value_size);
    if (err != PPDB_OK) {
        return err;
    }

    // Send response
    if (!p->parser.noreply) {
        return memcached_send_stored(conn);
    }
    return PPDB_OK;
}

// Handle DELETE command
static ppdb_error_t memcached_handle_delete(memcached_proto_t* p, ppdb_handle_t conn) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    
    // Delete value from storage
    ppdb_error_t err = ppdb_storage_delete(state->storage, p->parser.key, p->parser.key_size);
    if (err != PPDB_OK) {
        if (err == PPDB_ERR_NOT_FOUND) {
            if (!p->parser.noreply) {
                return memcached_send_not_found(conn);
            }
            return PPDB_OK;
        }
        return err;
    }

    if (!p->parser.noreply) {
        return memcached_send_deleted(conn);
    }
    return PPDB_OK;
}

// Handle incoming data
static ppdb_error_t memcached_proto_on_data(void* proto, ppdb_handle_t conn,
                                          const uint8_t* data, size_t size) {
    memcached_proto_t* p = proto;
    
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
    ppdb_error_t err = memcached_parse_command(p, p->buffer);
    if (err != PPDB_OK) {
        return err;
    }
    
    // Handle command
    switch (p->parser.type) {
        case MEMCACHED_CMD_GET:
            err = memcached_handle_get(p, conn);
            break;
            
        case MEMCACHED_CMD_SET:
            err = memcached_handle_set(p, conn);
            break;
            
        case MEMCACHED_CMD_DELETE:
            err = memcached_handle_delete(p, conn);
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
static const char* memcached_proto_get_name(void* proto) {
    PPDB_UNUSED(proto);
    return "memcached";
}

// Protocol operations table
static const peer_ops_t peer_memcached_ops = {
    .create = memcached_proto_create,
    .destroy = memcached_proto_destroy,
    .on_connect = memcached_proto_on_connect,
    .on_disconnect = memcached_proto_on_disconnect,
    .on_data = memcached_proto_on_data,
    .get_name = memcached_proto_get_name
};

// Protocol adapter getter
const peer_ops_t* peer_get_memcached(void) {
    return &peer_memcached_ops;
}

#endif // PPDB_PEER_MEMCACHED_INC_C_ 