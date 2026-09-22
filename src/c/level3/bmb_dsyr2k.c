#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"

/* dim1 = N, dim2 = K. C (N x N) = alpha*(A*B^T + B*A^T) + beta*C, A and B are N x K. */
typedef struct {
    int n;
    int k;
    double alpha;
    double beta;
    double *a; /* N x K */
    double *b; /* N x K */
    double *c; /* N x N, upper triangle used */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1, k = dim2;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->alpha = 1.0;
    ctx->beta = 0.0;
    ctx->a = malloc(n * k * sizeof(double));
    ctx->b = malloc(n * k * sizeof(double));
    ctx->c = malloc(n * n * sizeof(double));
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < n * k; i++) {
        ctx->a[i] = (double) (i % 100) * 0.01 - 0.5;
        ctx->b[i] = (double) ((i + 7) % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < n * n; i++) {
        ctx->c[i] = 0.0;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dsyr2k(CblasRowMajor, CblasUpper, CblasNoTrans, ctx->n, ctx->k,
                 ctx->alpha, ctx->a, ctx->k, ctx->b, ctx->k, ctx->beta, ctx->c, ctx->n);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b);
    free(ctx->c);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return 2.0 * d2 * d1 * (d1 + 1.0);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dsyr2k";
    bench.dim1_label = "Matrix dim1 (N)";
    bench.dim2_label = "Matrix dim2 (K)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.flops = flops;

    return bmb_benchmark_main(argc, argv, &bench);
}
