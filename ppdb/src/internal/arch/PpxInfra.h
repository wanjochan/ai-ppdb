#ifndef PPX_INFRA_H
#define PPX_INFRA_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

typedef struct {
    InfraxCore *core;   
    InfraxLog *logger;  
    // 未来可以方便地添加新组件
    // InfraxMemory *memory;
    // InfraxThread *thread;
} PpxInfra;

// 全局单例
const PpxInfra* ppx_infra(void);

#endif // PPX_INFRA_H