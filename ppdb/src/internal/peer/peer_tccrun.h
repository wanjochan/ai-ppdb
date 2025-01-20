#ifndef PEER_TCCRUN_H
#define PEER_TCCRUN_H

#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define TCCRUN_MAX_PATH_LEN 256
#define TCCRUN_MAX_ARGS 16

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

typedef struct {
    char source_path[TCCRUN_MAX_PATH_LEN];  // C源文件路径
    char* args[TCCRUN_MAX_ARGS];            // 运行参数
    int arg_count;                          // 参数数量
} tccrun_context_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 初始化 tccrun 服务
infra_error_t tccrun_init(const infra_config_t* config);

// 清理 tccrun 服务
infra_error_t tccrun_cleanup(void);

// 编译并运行 C 源文件
infra_error_t tccrun_execute(const char* source_path, int argc, char** argv);

// 命令行处理函数
infra_error_t tccrun_cmd_handler(int argc, char** argv);

#endif // PEER_TCCRUN_H 