#include "test/test_common.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_net.h"
#include "test/test_framework.h"
#include "test_macros.h"

TEST(test_peer_basic) {
    ppdb_peer_t* peer;
    ppdb_error_t err;

    // Test invalid parameters
    err = ppdb_peer_create(NULL, NULL);
    ASSERT_EQ(err, PPDB_ERR_NULL_POINTER);

    // Test create/destroy
    ppdb_peer_config_t config = {
        .host = "localhost",
        .port = 8080,
        .timeout_ms = 1000
    };

    err = ppdb_peer_create(&peer, &config);
    ASSERT_OK(err);
    ASSERT_NOT_NULL(peer);

    ppdb_peer_destroy(peer);
}

TEST(test_peer_connect) {
    ppdb_peer_t* peer;
    ppdb_error_t err;

    ppdb_peer_config_t config = {
        .host = "localhost",
        .port = 8080,
        .timeout_ms = 1000
    };

    err = ppdb_peer_create(&peer, &config);
    ASSERT_OK(err);

    // Test connect
    err = ppdb_peer_connect(peer);
    ASSERT_OK(err);

    // Test double connect
    err = ppdb_peer_connect(peer);
    ASSERT_EQ(err, PPDB_ERR_INVALID_STATE);

    // Test disconnect
    err = ppdb_peer_disconnect(peer);
    ASSERT_OK(err);

    ppdb_peer_destroy(peer);
}

TEST(test_peer_send_recv) {
    ppdb_peer_t* peer;
    ppdb_error_t err;
    const char* test_data = "Hello, World!";
    char recv_buf[256];
    size_t recv_size = sizeof(recv_buf);

    ppdb_peer_config_t config = {
        .host = "localhost",
        .port = 8080,
        .timeout_ms = 1000
    };

    err = ppdb_peer_create(&peer, &config);
    ASSERT_OK(err);

    // Connect first
    err = ppdb_peer_connect(peer);
    ASSERT_OK(err);

    // Test send
    err = ppdb_peer_send(peer, test_data, infra_strlen(test_data) + 1);
    ASSERT_OK(err);

    // Test receive
    err = ppdb_peer_recv(peer, recv_buf, sizeof(recv_buf), &recv_size);
    ASSERT_OK(err);
    ASSERT_EQ(recv_size, infra_strlen(test_data) + 1);
    ASSERT_EQ(infra_strcmp(recv_buf, test_data), 0);

    // Cleanup
    err = ppdb_peer_disconnect(peer);
    ASSERT_OK(err);

    ppdb_peer_destroy(peer);
}

TEST(test_peer_error_cases) {
    ppdb_peer_t* peer;
    ppdb_error_t err;
    char buf[256];
    size_t size;

    ppdb_peer_config_t config = {
        .host = "localhost",
        .port = 8080,
        .timeout_ms = 1000
    };

    err = ppdb_peer_create(&peer, &config);
    ASSERT_OK(err);

    // Test operations before connect
    err = ppdb_peer_send(peer, "test", 5);
    ASSERT_EQ(err, PPDB_ERR_INVALID_STATE);

    err = ppdb_peer_recv(peer, buf, sizeof(buf), &size);
    ASSERT_EQ(err, PPDB_ERR_INVALID_STATE);

    err = ppdb_peer_disconnect(peer);
    ASSERT_EQ(err, PPDB_ERR_INVALID_STATE);

    // Connect and test invalid parameters
    err = ppdb_peer_connect(peer);
    ASSERT_OK(err);

    err = ppdb_peer_send(peer, NULL, 5);
    ASSERT_EQ(err, PPDB_ERR_NULL_POINTER);

    err = ppdb_peer_recv(peer, NULL, sizeof(buf), &size);
    ASSERT_EQ(err, PPDB_ERR_NULL_POINTER);

    err = ppdb_peer_recv(peer, buf, sizeof(buf), NULL);
    ASSERT_EQ(err, PPDB_ERR_NULL_POINTER);

    // Cleanup
    err = ppdb_peer_disconnect(peer);
    ASSERT_OK(err);

    ppdb_peer_destroy(peer);
}

int main() {
    RUN_TEST(test_peer_basic);
    RUN_TEST(test_peer_connect);
    RUN_TEST(test_peer_send_recv);
    RUN_TEST(test_peer_error_cases);
    return 0;
}
