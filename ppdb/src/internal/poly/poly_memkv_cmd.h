#ifndef POLY_MEMKV_CMD_H
#define POLY_MEMKV_CMD_H

#include "internal/infra/infra_core.h"

// 初始化memkv命令行
infra_error_t poly_memkv_cmd_init(void);

// 清理memkv命令行
infra_error_t poly_memkv_cmd_cleanup(void);

#endif // POLY_MEMKV_CMD_H 