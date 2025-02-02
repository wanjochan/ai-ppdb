#include "now.h"
#include <sys/time.h>

int64_t dill_mnow(void) {
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    if(rc < 0) return -1;
    return ((int64_t)tv.tv_sec) * 1000 + (((int64_t)tv.tv_usec) / 1000);
}

int64_t dill_now_(void) {
    return dill_mnow();
}

int64_t dill_now(void) {
    return dill_mnow();
}

int dill_ctx_now_init(struct dill_ctx_now *ctx) {
    int64_t now = dill_mnow();
    if(now < 0) return -1;
    ctx->last_time = now;
    ctx->last_tsc = 0;
    return 0;
}

void dill_ctx_now_term(struct dill_ctx_now *ctx) {
    // 什么都不需要做
    (void)ctx;
}
