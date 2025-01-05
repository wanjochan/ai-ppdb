
//==============================================================================
// Peer API
//==============================================================================

// Peer 相关常量
#define PPDB_DEFAULT_PORT 11211
#define PPDB_MAX_COMMAND_LEN 1024
#define PPDB_MAX_VALUE_SIZE (1024 * 1024)  // 1MB

// Peer 角色和状态
typedef enum ppdb_peer_role_e {
    PPDB_PEER_SERVER = 1,
    PPDB_PEER_CLIENT = 2,
    PPDB_PEER_REPLICA = 3,     // 预留
    PPDB_PEER_CLUSTER = 4      // 预留
} ppdb_peer_role_t;

// Peer 错误码
typedef enum ppdb_peer_error_e {
    PPDB_PEER_OK = 0,
    PPDB_PEER_ERROR = -1,
    PPDB_PEER_AUTH_REQUIRED = -2,
    PPDB_PEER_INVALID_COMMAND = -3,
    PPDB_PEER_NETWORK_ERROR = -4
} ppdb_peer_error_t;

// Peer API 函数
// 创建peer（服务端/客户端）
ppdb_peer_t* ppdb_peer_create_server(ppdb_t* db, const char* host, int port);
ppdb_peer_t* ppdb_peer_create_client(void);
// 基础操作
int ppdb_peer_start(ppdb_peer_t* peer);
void ppdb_peer_stop(ppdb_peer_t* peer);
void ppdb_peer_free(ppdb_peer_t* peer);

// 客户端操作
int ppdb_peer_connect(ppdb_peer_t* peer, const char* host, int port);
int ppdb_peer_auth(ppdb_peer_t* peer, const char* user, const char* pass);
int ppdb_peer_execute(ppdb_peer_t* peer, const char* cmd);

// 为将来扩展预留的分布式接口
int ppdb_peer_join(ppdb_peer_t* peer, const char* cluster);
int ppdb_peer_replicate(ppdb_peer_t* peer, const char* master);
