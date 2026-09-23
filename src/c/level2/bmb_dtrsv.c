#include <cblas.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"

/* Square routine: only dim1 (= N) is used, swept via --matrix-dim1.
 * X is overwritten in place by cblas_dtrsv(); it is restored from x0
 * before every call so the benchmark is stable across --iterations. */
typedef struct {
    int n;
    double *a;  /* N x N, row-major, upper triangle used */
    double *x0; /* template */
    double *x;  /* working buffer, restored by reset() before each call */
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
    ctx->a = malloc(n * n * sizeof(double));
    ctx->x0 = malloc(n * sizeof(double));
    ctx->x = malloc(n * sizeof(double));
    if (ctx->a == NULL || ctx->x0 == NULL || ctx->x == NULL) {
        free(ctx->a);
        free(ctx->x0);
        free(ctx->x);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < n; i++) {
        size_t j;

        for (j = 0; j < n; j++) {
            /* Diagonally dominant so the triangular solve is well-conditioned. */
            ctx->a[i * n + j] = (i == j) ? (double) (n + 1) : (0.5 / (double) n);
        }
        ctx->x0[i] = (double) (i % 100) * 0.01 + 1.0;
    }
    memcpy(ctx->x, ctx->x0, n * sizeof(double));

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dtrsv(CblasRowMajor, CblasUpper, CblasNoTrans, CblasNonUnit, ctx->n, ctx->a, ctx->n, ctx->x, 1);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->x, ctx->x0, (size_t) ctx->n * sizeof(double));
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->x0);
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

    return 8.0 * (d1 * (d1 + 1.0) / 2.0 + 2.0 * d1);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dtrsv";
    bench.dim1_label = "Matrix dim (N)";
    bench.dim2_label = NULL;
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.reset = reset;
    bench.reset_every_call = 1;
    bench.teardown = teardown;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
