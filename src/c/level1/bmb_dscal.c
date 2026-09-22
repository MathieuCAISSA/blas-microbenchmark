#include <cblas.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"

/* X is scaled in place, so it shrinks by alpha at every call: over a long
 * --iterations run it would reach denormals and slow the routine down
 * mid-measurement. reset() restores it from a template, outside the timed
 * window. */
typedef struct {
    int n;
    double alpha;
    double *x0; /* template */
    double *x;  /* working buffer, restored by reset() before each call */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t i;

    (void) dim2;
    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) dim1;
    /* Deliberately not exactly 1.0: some BLAS implementations special-case
     * alpha == 1 as a no-op fast path, which would not reflect real work. */
    ctx->alpha = 0.999999;
    ctx->x0 = malloc(dim1 * sizeof(double));
    ctx->x = malloc(dim1 * sizeof(double));
    if (ctx->x0 == NULL || ctx->x == NULL) {
        free(ctx->x0);
        free(ctx->x);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < dim1; i++) {
        ctx->x0[i] = (double) (i % 100) * 0.01 + 1.0;
    }
    memcpy(ctx->x, ctx->x0, dim1 * sizeof(double));

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dscal(ctx->n, ctx->alpha, ctx->x, 1);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->x, ctx->x0, (size_t) ctx->n * sizeof(double));
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x0);
    free(ctx->x);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;

    return d1;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;

    return 16.0 * d1;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dscal";
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.setup = setup;
    bench.call = call;
    bench.reset = reset;
    bench.teardown = teardown;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
