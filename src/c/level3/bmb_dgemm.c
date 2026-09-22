#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"

/* dim1 = M = K, dim2 = N (K is tied to M so the two --matrix-dim options
 * remain sufficient to describe the three GEMM dimensions; see README.md). */
typedef struct {
    int m;
    int n;
    int k;
    double alpha;
    double beta;
    double *a; /* M x K */
    double *b; /* K x N */
    double *c; /* M x N */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2, k = dim1;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->alpha = 1.0;
    ctx->beta = 0.0;
    ctx->a = malloc(m * k * sizeof(double));
    ctx->b = malloc(k * n * sizeof(double));
    ctx->c = malloc(m * n * sizeof(double));
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m * k; i++) {
        ctx->a[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < k * n; i++) {
        ctx->b[i] = (double) ((i + 7) % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < m * n; i++) {
        ctx->c[i] = 0.0;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, ctx->m, ctx->n, ctx->k,
                ctx->alpha, ctx->a, ctx->k, ctx->b, ctx->n, ctx->beta, ctx->c, ctx->n);
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

    return 2.0 * d1 * d1 * d2;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dgemm";
    bench.dim1_label = "Matrix dim1 (M=K)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.flops = flops;

    return bmb_benchmark_main(argc, argv, &bench);
}
