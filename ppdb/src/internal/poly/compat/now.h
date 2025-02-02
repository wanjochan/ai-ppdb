#ifndef DILL_NOW_INCLUDED
#define DILL_NOW_INCLUDED

#include <stdint.h>
#include <time.h>

struct dill_ctx_now {
    int64_t last_time;
    uint64_t last_tsc;
};

int dill_ctx_now_init(struct dill_ctx_now *ctx);
void dill_ctx_now_term(struct dill_ctx_now *ctx);

/* Same as dill_now() except that it doesn't use the context.
   I.e. it can be called before calling dill_ctx_now_init(). */
int64_t dill_mnow(void);

#endif
