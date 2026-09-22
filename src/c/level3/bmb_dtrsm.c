#include <cblas.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"

/* Side = Left: A is M x M triangular, B and C share the M x N shape.
 * dim1 = M, dim2 = N. B is overwritten in place by cblas_dtrsm(); it is
 * restored from b0 before every call so the benchmark is stable across
 * --iterations. */
typedef struct {
    int m;
    int n;
    double alpha;
    double *a;  /* M x M, upper triangle used, diagonally dominant */
    double *b0; /* template */
    double *b;  /* working buffer, restored by reset() before each call */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->alpha = 1.0;
    ctx->a = malloc(m * m * sizeof(double));
    ctx->b0 = malloc(m * n * sizeof(double));
    ctx->b = malloc(m * n * sizeof(double));
    if (ctx->a == NULL || ctx->b0 == NULL || ctx->b == NULL) {
        free(ctx->a);
        free(ctx->b0);
        free(ctx->b);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m; i++) {
        size_t j;

        for (j = 0; j < m; j++) {
            ctx->a[i * m + j] = (i == j) ? (double) (m + 1) : (0.5 / (double) m);
        }
    }
    for (i = 0; i < m * n; i++) {
        ctx->b0[i] = (double) (i % 100) * 0.01 + 1.0;
    }
    memcpy(ctx->b, ctx->b0, m * n * sizeof(double));

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dtrsm(CblasRowMajor, CblasLeft, CblasUpper, CblasNoTrans, CblasNonUnit,
                ctx->m, ctx->n, ctx->alpha, ctx->a, ctx->m, ctx->b, ctx->n);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->b, ctx->b0, (size_t) ctx->m * (size_t) ctx->n * sizeof(double));
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b0);
    free(ctx->b);
    free(ctx);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dtrsm";
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.reset = reset;
    bench.teardown = teardown;

    return bmb_benchmark_main(argc, argv, &bench);
}
