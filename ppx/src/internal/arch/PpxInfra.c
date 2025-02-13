#include "cosmopolitan.h"
#include "PpxInfra.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxAsync.h"

static PpxInfra g_infra = {0};
static bool g_infra_initialized = false;

const PpxInfra* ppx_infra(void) {
    if (!g_infra_initialized) {
        // Initialize core components
        g_infra.core = InfraxCoreClass.singleton();
        g_infra.logger = InfraxLogClass.singleton();

        g_infra.logClass = &InfraxLogClass;
        g_infra.coreClass = &InfraxCoreClass;//for plugin and script
        g_infra.memoryClass = &InfraxMemoryClass;
        g_infra.threadClass = &InfraxThreadClass;
        g_infra.netClass = &InfraxNetClass;
        g_infra.syncClass = &InfraxSyncClass;
        g_infra.asyncClass = &InfraxAsyncClass;

        g_infra_initialized = true;
    }
    return &g_infra;
}
