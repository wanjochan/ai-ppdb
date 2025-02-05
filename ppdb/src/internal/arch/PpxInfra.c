#include "cosmopolitan.h"
#include "PpxInfra.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

static PpxInfra g_infra = {0};
static bool g_initialized = false;

const PpxInfra* ppx_infra(void) {
    if (!g_initialized) {
        g_infra.core = get_global_infrax_core();
        g_infra.logger = get_global_infrax_log();
        g_initialized = true;
    }
    return &g_infra;
}
