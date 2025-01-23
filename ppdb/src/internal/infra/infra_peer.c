//#include "cosmopolitan.h"

//Peers (instances across process and networks)

#define MAX_PEERS 128
#define INVALID_SOCKET -1

typedef struct {
    int socket;
    int type;  // SOCK_STREAM for TCP, SOCK_DGRAM for UDP
    struct sockaddr_in addr;
    int is_active;
} peer_connection_t;

// Connection pool
static peer_connection_t peer_pool[MAX_PEERS];
static int peer_count = 0;

// Initialize peer management
int infra_peer_init(void) {
    memset(peer_pool, 0, sizeof(peer_pool));
    peer_count = 0;
    return 0;
}

// Create new peer connection
int infra_peer_connect(const char* host, int port, int type) {
    if (peer_count >= MAX_PEERS) {
        return -1;
    }

    int sock = socket(AF_INET, type, 0);
    if (sock == INVALID_SOCKET) {
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    // Add to pool
    peer_pool[peer_count].socket = sock;
    peer_pool[peer_count].type = type;
    peer_pool[peer_count].addr = addr;
    peer_pool[peer_count].is_active = 1;

    return peer_count++;
}

// Close peer connection
void infra_peer_close(int peer_id) {
    if (peer_id < 0 || peer_id >= peer_count) {
        return;
    }

    if (peer_pool[peer_id].is_active) {
        close(peer_pool[peer_id].socket);
        peer_pool[peer_id].is_active = 0;
    }
}

// Send data to peer
int infra_peer_send(int peer_id, const void* data, size_t len) {
    if (peer_id < 0 || peer_id >= peer_count || !peer_pool[peer_id].is_active) {
        return -1;
    }

    return send(peer_pool[peer_id].socket, data, len, 0);
}

// Receive data from peer
int infra_peer_recv(int peer_id, void* buffer, size_t len) {
    if (peer_id < 0 || peer_id >= peer_count || !peer_pool[peer_id].is_active) {
        return -1;
    }

    return recv(peer_pool[peer_id].socket, buffer, len, 0);
}

// Clean up peer management
void infra_peer_cleanup(void) {
    for (int i = 0; i < peer_count; i++) {
        infra_peer_close(i);
    }
    peer_count = 0;
}
