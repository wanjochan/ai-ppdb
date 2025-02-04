#include "PpxInfra.h"
#include "PpxInfraCore.h"
#include "PpxInfraLog.h"

// Define a global singleton PpxInfra instance initialized only once.
// 全局对象只在启动时初始化，后续只需引用该对象，避免重复赋值 overhead.
static PpxInfra global_ppxInfra = {
    .core = get_global_infra_core(),
    .logger = get_global_infra_log()
    // .new   = infrax_core_new,   // Function pointer for creating a new instance
    // .free  = infrax_core_free,  // Function pointer for freeing the instance
    // .print = infrax_core_print  // Function pointer for printing the instance
};

// Public interface to retrieve the global PpxInfra instance.
PpxInfra* get_global_ppxInfra(void) {
    return &global_ppxInfra;
}
