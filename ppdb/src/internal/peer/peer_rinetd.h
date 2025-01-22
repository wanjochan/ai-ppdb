#ifndef PEER_RINETD_H
#define PEER_RINETD_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
//#include "internal/infra/infra_mux.h"
#include "internal/poly/poly_cmdline.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define RINETD_BUFFER_SIZE 8192        // 转发缓冲区大小
#define RINETD_MAX_ADDR_LEN 256        // 地址最大长度
#define RINETD_MAX_PATH_LEN 256        // 路径最大长度
#define RINETD_MIN_THREADS 32           // 最小线程数
#define RINETD_MAX_THREADS 512         // 最大线程数
#define RINETD_MAX_RULES 128           // 最大规则数

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 转发规则
typedef struct {
    char src_addr[RINETD_MAX_ADDR_LEN];  // 源地址
    int src_port;                        // 源端口
    char dst_addr[RINETD_MAX_ADDR_LEN];  // 目标地址
    int dst_port;                        // 目标端口
} rinetd_rule_t;

// 监听线程参数
typedef struct {
    rinetd_rule_t* rule;                // 关联的规则
    infra_socket_t listener;            // 监听socket
} listener_thread_param_t;

// 转发连接
typedef struct {
    infra_socket_t client;              // 客户端socket
    infra_socket_t server;              // 服务端socket
    rinetd_rule_t* rule;                // 关联的规则
    char buffer[RINETD_BUFFER_SIZE];    // 转发缓冲区
} rinetd_conn_t;

// 转发会话
typedef struct {
    rinetd_conn_t* conn;                // 转发连接
    bool active;                        // 是否活跃
} rinetd_session_t;

// 服务上下文
typedef struct {
    bool running;                        // 服务是否运行
    char config_path[RINETD_MAX_PATH_LEN]; // 配置文件路径
    rinetd_rule_t* rules;               // 转发规则数组
    int rule_count;                      // 规则数量
    infra_thread_pool_t* pool;          // 线程池
    infra_socket_t* listeners;          // 监听socket数组
    infra_thread_t* listener_threads;    // 监听线程数组
    infra_mutex_t* mutex;               // 全局互斥锁
    rinetd_session_t* active_sessions;  // 活跃会话数组
    int session_count;                  // 会话数量
} rinetd_context_t;

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

// 初始化服务
infra_error_t rinetd_init(const infra_config_t* config);

// 清理服务
infra_error_t rinetd_cleanup(void);

// 加载配置
infra_error_t rinetd_load_config(const char* path);

// 保存配置
infra_error_t rinetd_save_config(const char* path);

// 启动服务
infra_error_t rinetd_start(void);

// 停止服务
infra_error_t rinetd_stop(void);

// 检查服务状态
bool rinetd_is_running(void);

// 命令行选项和处理函数
extern const poly_cmd_option_t rinetd_options[];
extern const int rinetd_option_count;
infra_error_t rinetd_cmd_handler(int argc, char** argv);

#endif // PEER_RINETD_H 
