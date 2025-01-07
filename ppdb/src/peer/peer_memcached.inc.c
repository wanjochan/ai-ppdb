#ifndef PPDB_PEER_MEMCACHED_INC_C_
#define PPDB_PEER_MEMCACHED_INC_C_

#include <cosmopolitan.h>
#include "../internal/peer.h"
#include "../internal/storage.h"

// Buffer size for responses
#define MEMCACHED_RESPONSE_BUFFER_SIZE 1024

// Memcached command states
typedef enum {
    MEMCACHED_STATE_COMMAND,  // Waiting for command line
    MEMCACHED_STATE_DATA      // Waiting for data block
} memcached_state_t;

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
    memcached_state_t state;
    char* key;
    size_t key_size;
    char* value;
    size_t value_size;
    size_t bytes_remaining;  // Remaining bytes to read for SET data
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
    fprintf(stderr, "Sending error: %s\n", msg);
    return ppdb_conn_send(conn, buf, len);
}

static ppdb_error_t memcached_send_stored(ppdb_handle_t conn) {
    fprintf(stderr, "Sending STORED response\n");
    return ppdb_conn_send(conn, "STORED\r\n", 8);
}

static ppdb_error_t memcached_send_not_stored(ppdb_handle_t conn) {
    fprintf(stderr, "Sending NOT_STORED response\n");
    return ppdb_conn_send(conn, "NOT_STORED\r\n", 12);
}

static ppdb_error_t memcached_send_deleted(ppdb_handle_t conn) {
    fprintf(stderr, "Sending DELETED response\n");
    return ppdb_conn_send(conn, "DELETED\r\n", 9);
}

static ppdb_error_t memcached_send_not_found(ppdb_handle_t conn) {
    fprintf(stderr, "Sending NOT_FOUND response\n");
    return ppdb_conn_send(conn, "NOT_FOUND\r\n", 11);
}

static ppdb_error_t memcached_send_value(ppdb_handle_t conn, const char* key, size_t key_size,
                                     const void* value, size_t value_size, uint32_t flags) {
    char header[MEMCACHED_RESPONSE_BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header), "VALUE %.*s %u %zu\r\n",
                            (int)key_size, key, flags, value_size);
    
    fprintf(stderr, "Sending value for key %.*s (size: %zu)\n", (int)key_size, key, value_size);
    
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
        fprintf(stderr, "Failed to allocate protocol instance\n");
        return PPDB_ERR_MEMORY;
    }
    p->parser.state = MEMCACHED_STATE_COMMAND;
    fprintf(stderr, "Created memcached protocol instance\n");
    *proto = p;
    return PPDB_OK;
}

// Destroy protocol instance
static void memcached_proto_destroy(void* proto) {
    fprintf(stderr, "Destroying memcached protocol instance\n");
    free(proto);
}

// Handle connection established
static ppdb_error_t memcached_proto_on_connect(void* proto, ppdb_handle_t conn) {
    PPDB_UNUSED(conn);
    memcached_proto_t* p = proto;
    p->buffer_size = 0;
    p->parser.state = MEMCACHED_STATE_COMMAND;
    fprintf(stderr, "New memcached connection established\n");
    return PPDB_OK;
}

// Handle connection closed
static void memcached_proto_on_disconnect(void* proto, ppdb_handle_t conn) {
    PPDB_UNUSED(conn);
    memcached_proto_t* p = proto;
    p->buffer_size = 0;
    p->parser.state = MEMCACHED_STATE_COMMAND;
    fprintf(stderr, "Memcached connection closed\n");
}

// Parse command line
static ppdb_error_t memcached_parse_command(memcached_proto_t* p, char* line) {
    fprintf(stderr, "Parsing command: %s\n", line);
    
    char* cmd = strtok(line, " ");
    if (!cmd) {
        fprintf(stderr, "Failed to parse command: empty line\n");
        return PPDB_ERR_PROTOCOL;
    }

    if (strcmp(cmd, "get") == 0) {
        p->parser.type = MEMCACHED_CMD_GET;
        char* key = strtok(NULL, " ");
        if (!key) {
            fprintf(stderr, "Failed to parse GET command: missing key\n");
            return PPDB_ERR_PROTOCOL;
        }
        p->parser.key = key;
        p->parser.key_size = strlen(key);
        fprintf(stderr, "Parsed GET command for key: %s\n", key);
    }
    else if (strcmp(cmd, "set") == 0) {
        p->parser.type = MEMCACHED_CMD_SET;
        char* key = strtok(NULL, " ");
        char* flags = strtok(NULL, " ");
        char* exptime = strtok(NULL, " ");
        char* bytes = strtok(NULL, " ");
        char* noreply = strtok(NULL, " ");
        
        if (!key || !flags || !exptime || !bytes) {
            fprintf(stderr, "Failed to parse SET command: missing parameters\n");
            return PPDB_ERR_PROTOCOL;
        }
        
        p->parser.key = key;
        p->parser.key_size = strlen(key);
        p->parser.flags = atoi(flags);
        p->parser.exptime = atoi(exptime);
        p->parser.bytes_remaining = atoi(bytes);
        p->parser.noreply = noreply ? true : false;
        p->parser.state = MEMCACHED_STATE_DATA;
        
        fprintf(stderr, "Parsed SET command for key: %s (size: %zu)\n", 
                key, p->parser.bytes_remaining);
    }
    else if (strcmp(cmd, "delete") == 0) {
        p->parser.type = MEMCACHED_CMD_DELETE;
        char* key = strtok(NULL, " ");
        char* noreply = strtok(NULL, " ");
        
        if (!key) {
            fprintf(stderr, "Failed to parse DELETE command: missing key\n");
            return PPDB_ERR_PROTOCOL;
        }
        
        p->parser.key = key;
        p->parser.key_size = strlen(key);
        p->parser.noreply = noreply ? true : false;
        
        fprintf(stderr, "Parsed DELETE command for key: %s\n", key);
    }
    else {
        p->parser.type = MEMCACHED_CMD_UNKNOWN;
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return PPDB_ERR_PROTOCOL;
    }
    
    return PPDB_OK;
}

// Handle GET command
static ppdb_error_t memcached_handle_get(memcached_proto_t* p, ppdb_handle_t conn) {
    fprintf(stderr, "Handling GET command for key: %s\n", p->parser.key);
    
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    size_t value_size = 0;
    char value_buffer[MEMCACHED_RESPONSE_BUFFER_SIZE];
    
    // Get value from storage
    ppdb_error_t err = ppdb_storage_get(state->storage, p->parser.key, p->parser.key_size,
                                      value_buffer, &value_size);
    if (err != PPDB_OK) {
        if (err == PPDB_ERR_NOT_FOUND) {
            fprintf(stderr, "Key not found: %s\n", p->parser.key);
            return ppdb_conn_send(conn, "END\r\n", 5);
        }
        fprintf(stderr, "Storage error during GET: %d\n", err);
        return err;
    }

    fprintf(stderr, "Found value for key %s (size: %zu)\n", p->parser.key, value_size);
    return memcached_send_value(conn, p->parser.key, p->parser.key_size, value_buffer, value_size, p->parser.flags);
}

// Handle SET command
static ppdb_error_t memcached_handle_set(memcached_proto_t* p, ppdb_handle_t conn) {
    fprintf(stderr, "Handling SET command for key: %s (value size: %zu)\n", 
            p->parser.key, p->parser.value_size);
    
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    
    // Store value in storage
    ppdb_error_t err = ppdb_storage_put(state->storage, p->parser.key, p->parser.key_size,
                                      p->parser.value, p->parser.value_size);
    if (err != PPDB_OK) {
        fprintf(stderr, "Storage error during SET: %d\n", err);
        return err;
    }

    fprintf(stderr, "Successfully stored value for key: %s\n", p->parser.key);
    
    // Send response
    if (!p->parser.noreply) {
        return memcached_send_stored(conn);
    }
    return PPDB_OK;
}

// Handle DELETE command
static ppdb_error_t memcached_handle_delete(memcached_proto_t* p, ppdb_handle_t conn) {
    fprintf(stderr, "Handling DELETE command for key: %s\n", p->parser.key);
    
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    
    // Delete value from storage
    ppdb_error_t err = ppdb_storage_delete(state->storage, p->parser.key, p->parser.key_size);
    if (err != PPDB_OK) {
        if (err == PPDB_ERR_NOT_FOUND) {
            fprintf(stderr, "Key not found for deletion: %s\n", p->parser.key);
            if (!p->parser.noreply) {
                return memcached_send_not_found(conn);
            }
            return PPDB_OK;
        }
        fprintf(stderr, "Storage error during DELETE: %d\n", err);
        return err;
    }

    fprintf(stderr, "Successfully deleted key: %s\n", p->parser.key);
    
    if (!p->parser.noreply) {
        return memcached_send_deleted(conn);
    }
    return PPDB_OK;
}

// Handle incoming data
static ppdb_error_t memcached_proto_on_data(void* proto, ppdb_handle_t conn,
                                          const uint8_t* data, size_t size) {
    memcached_proto_t* p = proto;
    
    fprintf(stderr, "Received %zu bytes of data\n", size);
    
    // Append to buffer
    if (p->buffer_size + size > sizeof(p->buffer)) {
        fprintf(stderr, "Buffer overflow: buffer_size=%zu, incoming_size=%zu, max_size=%zu\n",
                p->buffer_size, size, sizeof(p->buffer));
        return PPDB_ERR_BUFFER_FULL;
    }
    memcpy(p->buffer + p->buffer_size, data, size);
    p->buffer_size += size;
    
    ppdb_error_t err = PPDB_OK;
    
    while (p->buffer_size > 0) {
        if (p->parser.state == MEMCACHED_STATE_COMMAND) {
            // Find command line
            char* newline = memchr(p->buffer, '\n', p->buffer_size);
            if (!newline) {
                fprintf(stderr, "Incomplete command, waiting for more data\n");
                return PPDB_OK; // Need more data
            }
            
            // Parse command
            *newline = '\0';
            if (newline > p->buffer && *(newline - 1) == '\r') {
                *(newline - 1) = '\0';
            }
            
            err = memcached_parse_command(p, p->buffer);
            if (err != PPDB_OK) {
                fprintf(stderr, "Failed to parse command: %d\n", err);
                return err;
            }
            
            // If not SET command, handle it now
            if (p->parser.type != MEMCACHED_CMD_SET) {
                switch (p->parser.type) {
                    case MEMCACHED_CMD_GET:
                        err = memcached_handle_get(p, conn);
                        break;
                        
                    case MEMCACHED_CMD_DELETE:
                        err = memcached_handle_delete(p, conn);
                        break;
                        
                    default:
                        fprintf(stderr, "Unknown command type: %d\n", p->parser.type);
                        err = PPDB_ERR_PROTOCOL;
                        break;
                }
                
                if (err != PPDB_OK) {
                    fprintf(stderr, "Command handling failed: %d\n", err);
                    return err;
                }
                
                // Move remaining data to start of buffer
                size_t remaining = p->buffer_size - (newline - p->buffer + 1);
                if (remaining > 0) {
                    memmove(p->buffer, newline + 1, remaining);
                }
                p->buffer_size = remaining;
            } else {
                // For SET command, move to data state and wait for data
                size_t remaining = p->buffer_size - (newline - p->buffer + 1);
                if (remaining > 0) {
                    memmove(p->buffer, newline + 1, remaining);
                    p->buffer_size = remaining;
                } else {
                    p->buffer_size = 0;
                }
            }
        } else if (p->parser.state == MEMCACHED_STATE_DATA) {
            // Check if we have enough data
            if (p->buffer_size < p->parser.bytes_remaining + 2) { // +2 for \r\n
                fprintf(stderr, "Waiting for more data: have %zu, need %zu\n",
                        p->buffer_size, p->parser.bytes_remaining + 2);
                return PPDB_OK;
            }
            
            // Store the value
            p->parser.value = p->buffer;
            p->parser.value_size = p->parser.bytes_remaining;
            
            // Handle SET command
            err = memcached_handle_set(p, conn);
            if (err != PPDB_OK) {
                fprintf(stderr, "SET command handling failed: %d\n", err);
                return err;
            }
            
            // Move remaining data to start of buffer
            size_t remaining = p->buffer_size - (p->parser.bytes_remaining + 2);
            if (remaining > 0) {
                memmove(p->buffer, p->buffer + p->parser.bytes_remaining + 2, remaining);
            }
            p->buffer_size = remaining;
            
            // Reset state
            p->parser.state = MEMCACHED_STATE_COMMAND;
        }
    }
    
    fprintf(stderr, "Command handled successfully, remaining buffer size: %zu\n", p->buffer_size);
    return PPDB_OK;
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