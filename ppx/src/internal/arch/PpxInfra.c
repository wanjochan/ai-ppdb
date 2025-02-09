#include "cosmopolitan.h"
#include "PpxInfra.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

static PpxInfra g_infra = {0};
static bool g_infra_initialized = false;

const PpxInfra* ppx_infra(void) {
    if (!g_infra_initialized) {
        // Initialize core components
        g_infra.core = InfraxCoreClass.singleton();
        g_infra.logger = InfraxLogClass.singleton();
        g_infra_initialized = true;
    }
    return &g_infra;
}
