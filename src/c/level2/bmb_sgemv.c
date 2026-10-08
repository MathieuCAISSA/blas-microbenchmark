#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* dim1 = M (rows of A, length of Y), dim2 = N (columns of A, length of X). */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int m;
    int n;
    float alpha;
    float beta;
    float *a; /* M x N, in the order of --layout */
    float *x; /* N */
    float *y; /* M */
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
    ctx->alpha = 1.0f;
    ctx->beta = 0.0f;
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = ctx->col ? ctx->m : ctx->n;
    ctx->a = malloc(dim1 * dim2 * sizeof(float));
    ctx->x = malloc(dim2 * sizeof(float));
    ctx->y = malloc(dim1 * sizeof(float));
    if (ctx->a == NULL || ctx->x == NULL || ctx->y == NULL) {
        free(ctx->a);
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < dim1 * dim2; i++) {
        ctx->a[i] = (float) (i % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < dim2; i++) {
        ctx->x[i] = (float) (i % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < dim1; i++) {
        ctx->y[i] = 0.0f;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_sgemv(ctx->order, CblasNoTrans, ctx->m, ctx->n, ctx->alpha,
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

    return 4.0 * (d1 * d2 + d2 + 2.0 * d1);
}

/* --verify: alpha * A * x + beta * y0, recomputed in double. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, i;
    double *y0 = malloc((6 * m + n) * sizeof(double));
    int ok;

    if (y0 == NULL) {
        return -1;
    }
    double *t = y0 + m, *tabs = y0 + 2 * m, *want = y0 + 3 * m, *scale = y0 + 4 * m;
    double *got = y0 + 5 * m, *x = y0 + 6 * m;

    for (i = 0; i < m; i++) {
        y0[i] = ctx->y[i];
    }
    for (i = 0; i < n; i++) {
        x[i] = ctx->x[i];
    }
    call(ctx);
    bmb_verify_perturb_single(&ctx->y[0]);
    bmb_verify_matvec_single(ctx->col, m, n, ctx->a, (size_t) ctx->lda, x, NULL, t, tabs);
    for (i = 0; i < m; i++) {
        got[i] = ctx->y[i];
        want[i] = ctx->alpha * t[i] + ctx->beta * y0[i];
        scale[i] = fabs(ctx->alpha) * tabs[i] + fabs(ctx->beta * y0[i]);
    }
    ok = bmb_verify_close("y", got, want, scale, m, bmb_verify_tolerance_single(n), msg, size);
    free(y0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "sgemv";
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
