#ifndef PPX_INFRA_H
#define PPX_INFRA_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

typedef struct {
    // Core components
    InfraxCore *core;   
    InfraxCoreClassType *coreClass;  // 使用正确的类型名

    // Logging components
    InfraxLog *logger;  

    //InfraxMemoryClassType InfraxMemoryClass;
    //InfraxMemory *memory;

    // 未来可以方便地添加新组件
    // InfraxMemory *memory;
    // InfraxThread *thread;
} PpxInfra;

// 全局单例（将来会用于给插件、脚本调用）
const PpxInfra* ppx_infra(void);

#endif // PPX_INFRA_H