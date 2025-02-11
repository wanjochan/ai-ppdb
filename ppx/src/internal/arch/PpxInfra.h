#ifndef PPX_INFRA_H
#define PPX_INFRA_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxAsync.h"

typedef struct {
    InfraxCore *core;   
    InfraxLog *logger;  

    InfraxLogClassType* logClass;
    InfraxCoreClassType* coreClass;
    InfraxMemoryClassType* memoryClass;
    InfraxThreadClassType* threadClass;
    InfraxSocketClassType* socketClass;
    InfraxSyncClassType* syncClass;
    InfraxAsyncClassType* asyncClass;
} PpxInfra;

// 全局单例（将来会用于给插件、脚本调用）
const PpxInfra* ppx_infra(void);

#endif // PPX_INFRA_H