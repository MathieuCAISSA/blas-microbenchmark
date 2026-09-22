#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"

/* dim1 = M (rows of A, length of Y), dim2 = N (columns of A, length of X). */
typedef struct {
    int m;
    int n;
    double alpha;
    double beta;
    double *a; /* M x N, row-major */
    double *x; /* N */
    double *y; /* M */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) dim1;
    ctx->n = (int) dim2;
    ctx->alpha = 1.0;
    ctx->beta = 0.0;
    ctx->a = malloc(dim1 * dim2 * sizeof(double));
    ctx->x = malloc(dim2 * sizeof(double));
    ctx->y = malloc(dim1 * sizeof(double));
    if (ctx->a == NULL || ctx->x == NULL || ctx->y == NULL) {
        free(ctx->a);
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < dim1 * dim2; i++) {
        ctx->a[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < dim2; i++) {
        ctx->x[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < dim1; i++) {
        ctx->y[i] = 0.0;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dgemv(CblasRowMajor, CblasNoTrans, ctx->m, ctx->n, ctx->alpha,
                ctx->a, ctx->n, ctx->x, 1, ctx->beta, ctx->y, 1);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return 2.0 * d1 * d2;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return 8.0 * (d1 * d2 + d2 + 2.0 * d1);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dgemv";
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
