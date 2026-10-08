#include <cblas.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* dim1 = M (rows of A, length of Y), dim2 = N (columns of A, length of X). */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int m;
    int n;
    double alpha;
    double beta;
    double *a; /* M x N, in the order of --layout */
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
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = ctx->col ? ctx->m : ctx->n;
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

    cblas_dgemv(ctx->order, CblasNoTrans, ctx->m, ctx->n, ctx->alpha,
                ctx->a, ctx->lda, ctx->x, 1, ctx->beta, ctx->y, 1);
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

/* --verify: alpha * A * x + beta * y0, recomputed. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, i;
    double *y0 = malloc(5 * m * sizeof(double));
    int ok;

    if (y0 == NULL) {
        return -1;
    }
    double *t = y0 + m, *tabs = y0 + 2 * m, *want = y0 + 3 * m, *scale = y0 + 4 * m;

    memcpy(y0, ctx->y, m * sizeof(double));
    call(ctx);
    bmb_verify_perturb(&ctx->y[0]);
    bmb_verify_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->a, ctx->lda, ctx->x, NULL, t, tabs);
    for (i = 0; i < m; i++) {
        want[i] = ctx->alpha * t[i] + ctx->beta * y0[i];
        scale[i] = fabs(ctx->alpha) * tabs[i] + fabs(ctx->beta * y0[i]);
    }
    ok = bmb_verify_close("y", ctx->y, want, scale, m, bmb_verify_tolerance(n), msg, size);
    free(y0);
    return !ok;
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
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
