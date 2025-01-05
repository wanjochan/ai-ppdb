/*
 * peer.c - PPDB对等节点实现
 *
 * 本文件是PPDB对等节点层的主入口，负责组织和初始化所有对等节点模块。
 */

#include <cosmopolitan.h>
#include <internal/base.h>
#include <internal/peer.h>

// Internal types
typedef struct ppdb_peer {
    ppdb_peer_config_t config;
    ppdb_engine_async_loop_t* loop;
    ppdb_peer_connection_t* conn;
    ppdb_engine_mutex_t* mutex;
    bool initialized;
} ppdb_peer_t;

// Implementation of peer functions
ppdb_error_t ppdb_peer_create(ppdb_peer_t** peer, const ppdb_peer_config_t* config) {
    ppdb_error_t err;
    ppdb_peer_t* p;

    if (!peer || !config) {
        return PPDB_ERR_NULL_POINTER;
    }

    // Allocate peer structure
    p = ppdb_engine_malloc(sizeof(ppdb_peer_t));
    if (!p) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Initialize fields
    memcpy(&p->config, config, sizeof(ppdb_peer_config_t));
    p->initialized = false;

    // Create async loop
    err = ppdb_engine_async_loop_create(&p->loop);
    if (err != PPDB_OK) {
        ppdb_engine_free(p);
        return err;
    }

    // Create mutex
    err = ppdb_engine_mutex_create(&p->mutex);
    if (err != PPDB_OK) {
        ppdb_engine_async_loop_destroy(p->loop);
        ppdb_engine_free(p);
        return err;
    }

    *peer = p;
    return PPDB_OK;
}

void ppdb_peer_destroy(ppdb_peer_t* peer) {
    if (!peer) {
        return;
    }

    // Cleanup connection if exists
    if (peer->conn) {
        ppdb_peer_connection_destroy(peer->conn);
    }

    // Cleanup async loop
    if (peer->loop) {
        ppdb_engine_async_loop_destroy(peer->loop);
    }

    // Cleanup mutex
    if (peer->mutex) {
        ppdb_engine_mutex_destroy(peer->mutex);
    }

    ppdb_engine_free(peer);
}

ppdb_error_t ppdb_peer_connect(ppdb_peer_t* peer) {
    ppdb_error_t err;

    if (!peer) {
        return PPDB_ERR_NULL_POINTER;
    }

    err = ppdb_engine_mutex_lock(peer->mutex);
    if (err != PPDB_OK) {
        return err;
    }

    if (peer->initialized) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    // Create connection
    err = ppdb_peer_connection_create(peer->loop, &peer->conn);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return err;
    }

    // Connect to remote peer
    err = ppdb_peer_connection_connect(peer->conn, peer->config.host, peer->config.port);
    if (err != PPDB_OK) {
        ppdb_peer_connection_destroy(peer->conn);
        peer->conn = NULL;
        ppdb_engine_mutex_unlock(peer->mutex);
        return err;
    }

    peer->initialized = true;
    ppdb_engine_mutex_unlock(peer->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_disconnect(ppdb_peer_t* peer) {
    ppdb_error_t err;

    if (!peer) {
        return PPDB_ERR_NULL_POINTER;
    }

    err = ppdb_engine_mutex_lock(peer->mutex);
    if (err != PPDB_OK) {
        return err;
    }

    if (!peer->initialized) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    // Disconnect and cleanup connection
    if (peer->conn) {
        err = ppdb_peer_connection_disconnect(peer->conn);
        if (err != PPDB_OK) {
            ppdb_engine_mutex_unlock(peer->mutex);
            return err;
        }

        ppdb_peer_connection_destroy(peer->conn);
        peer->conn = NULL;
    }

    peer->initialized = false;
    ppdb_engine_mutex_unlock(peer->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_send(ppdb_peer_t* peer, const void* buf, size_t size) {
    ppdb_error_t err;
    ppdb_peer_msg_header_t header;

    if (!peer || !buf) {
        return PPDB_ERR_NULL_POINTER;
    }

    err = ppdb_engine_mutex_lock(peer->mutex);
    if (err != PPDB_OK) {
        return err;
    }

    if (!peer->initialized || !peer->conn) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    // Prepare message header
    header.magic = 0x50504442;  // "PPDB"
    header.version = 1;
    header.type = PPDB_PEER_MSG_DATA;
    header.payload_size = size;

    // Send message
    err = ppdb_peer_msg_send(peer->conn, PPDB_PEER_MSG_DATA, buf, size);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return err;
    }

    ppdb_engine_mutex_unlock(peer->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_recv(ppdb_peer_t* peer, void* buf, size_t size, size_t* received) {
    ppdb_error_t err;
    ppdb_peer_msg_header_t header;

    if (!peer || !buf || !received) {
        return PPDB_ERR_NULL_POINTER;
    }

    err = ppdb_engine_mutex_lock(peer->mutex);
    if (err != PPDB_OK) {
        return err;
    }

    if (!peer->initialized || !peer->conn) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    // Receive message
    err = ppdb_peer_msg_recv(peer->conn, &header, buf, received);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return err;
    }

    // Verify message
    if (header.magic != 0x50504442) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    if (header.type != PPDB_PEER_MSG_DATA) {
        ppdb_engine_mutex_unlock(peer->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    ppdb_engine_mutex_unlock(peer->mutex);
    return PPDB_OK;
}
