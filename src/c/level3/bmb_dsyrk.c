#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* dim1 = N, dim2 = K. C (N x N) = alpha * A * A^T + beta * C, A is N x K. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
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
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = ctx->col ? ctx->n : ctx->k;
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

    cblas_dsyrk(ctx->order, CblasUpper, CblasNoTrans, ctx->n, ctx->k,
                ctx->alpha, ctx->a, ctx->lda, ctx->beta, ctx->c, ctx->n);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->c);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return d2 * d1 * (d1 + 1.0);
}

/* --verify, by random projection: Sym(C)*r against alpha*A*(A'*r) + beta*Sym(C0)*r,
 * Sym(C) being the symmetric matrix C's upper triangle stands for. The
 * strictly lower part, which the routine must not touch, is projected
 * before and after the call, and must come out the same to the bit. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, k = (size_t) ctx->k, i;
    double *r = malloc((15 * n + 2 * k) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + n, *tabs = t + k, *c0 = tabs + k, *c0abs = c0 + n;
    double *y1 = c0abs + n, *y1abs = y1 + n, *y2 = y1abs + n, *y2abs = y2 + n;
    double *u = y2abs + n, *uabs = u + n, *l0 = uabs + n, *l = l0 + n, *labs = l + n;
    double *want = labs + n, *scale = want + n;

    (void) u;
    (void) uabs;
    bmb_verify_probe(r, n);
    for (i = 0; i < n; i++) {
        c0[i] = c0abs[i] = 0.0;
    }
    if (ctx->beta != 0.0) {
        bmb_verify_matvec(BMB_VERIFY_SYM_UPPER, ctx->col, n, n, ctx->c, n, r, NULL, c0, c0abs);
    }
    bmb_verify_matvec(BMB_VERIFY_LOWER_STRICT, ctx->col, n, n, ctx->c, n, r, NULL, l0, labs);
    call(ctx);
    bmb_verify_perturb(&ctx->c[0]);
    bmb_verify_matvec(BMB_VERIFY_SYM_UPPER, ctx->col, n, n, ctx->c, n, r, NULL, y1, y1abs);
    bmb_verify_matvec(BMB_VERIFY_FULL_T, ctx->col, n, k, ctx->a, ctx->lda, r, NULL, t, tabs);
    bmb_verify_matvec(BMB_VERIFY_FULL, ctx->col, n, k, ctx->a, ctx->lda, t, tabs, y2, y2abs);
    for (i = 0; i < n; i++) {
        want[i] = ctx->alpha * y2[i] + ctx->beta * c0[i];
        scale[i] = y1abs[i] + fabs(ctx->alpha) * y2abs[i] + fabs(ctx->beta) * c0abs[i];
    }
    ok = bmb_verify_close("C*r, for a random vector r", y1, want, scale, n,
                          bmb_verify_tolerance(k + n), msg, size);
    if (ok) {
        bmb_verify_matvec(BMB_VERIFY_LOWER_STRICT, ctx->col, n, n, ctx->c, n, r, NULL, l, labs);
        ok = bmb_verify_close("the strictly lower part of C, times r, which the call must not change",
                              l, l0, NULL, n, 0.0, msg, size);
    }
    free(r);
    return !ok;
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
    bench.verify = verify;
    bench.flops = flops;

    return bmb_benchmark_main(argc, argv, &bench);
}
