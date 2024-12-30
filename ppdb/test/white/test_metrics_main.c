#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/metrics.h"

extern void test_metrics_basic();
extern void test_metrics_concurrent();
extern void test_metrics_accuracy();
extern void test_memtable_metrics();

int main() {
    TEST_INIT("Metrics Tests");

    TEST_RUN(test_metrics_basic);
    TEST_RUN(test_metrics_concurrent);
    TEST_RUN(test_metrics_accuracy);
    TEST_RUN(test_memtable_metrics);

    TEST_REPORT();
    return 0;
}
