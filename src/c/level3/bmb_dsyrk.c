#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"

/* dim1 = N, dim2 = K. C (N x N) = alpha * A * A^T + beta * C, A is N x K. */
typedef struct {
    int n;
    int k;
    double alpha;
    double beta;
    double *a; /* N x K */
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
    ctx->c = malloc(n * n * sizeof(double));
    if (ctx->a == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < n * k; i++) {
        ctx->a[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < n * n; i++) {
        ctx->c[i] = 0.0;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dsyrk(CblasRowMajor, CblasUpper, CblasNoTrans, ctx->n, ctx->k,
                ctx->alpha, ctx->a, ctx->k, ctx->beta, ctx->c, ctx->n);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->c);
    free(ctx);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dsyrk";
    bench.dim1_label = "Matrix dim1 (N)";
    bench.dim2_label = "Matrix dim2 (K)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;

    return bmb_benchmark_main(argc, argv, &bench);
}
