#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"

/* Square routine: only dim1 (= N) is used, swept via --matrix-dim1.
 * A (N x N, upper triangle) is updated in place: A += alpha * x * x^T. */
typedef struct {
    int n;
    double alpha;
    double *a;
    double *x;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1;
    size_t i;

    (void) dim2;
    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    /* Small alpha: A accumulates rank-1 updates across iterations. */
    ctx->alpha = 1.0e-6;
    ctx->a = malloc(n * n * sizeof(double));
    ctx->x = malloc(n * sizeof(double));
    if (ctx->a == NULL || ctx->x == NULL) {
        free(ctx->a);
        free(ctx->x);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < n * n; i++) {
        ctx->a[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < n; i++) {
        ctx->x[i] = (double) (i % 100) * 0.01 - 0.5;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dsyr(CblasRowMajor, CblasUpper, ctx->n, ctx->alpha, ctx->x, 1, ctx->a, ctx->n);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->x);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;

    return d1 * (d1 + 1.0);
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;

    return 8.0 * (d1 * (d1 + 1.0) + d1);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dsyr";
    bench.dim1_label = "Matrix dim (N)";
    bench.dim2_label = NULL;
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
