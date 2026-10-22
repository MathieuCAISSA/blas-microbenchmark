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
    int ldc;
    int n;
    int k;
    float alpha;
    float beta;
    float *a; /* N x K */
    float *c; /* N x N, upper triangle used */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1, k = dim2;
    const int col = bmb_bench_column_major();
    /* A as stored: its rows, or its columns with --layout col. */
    const size_t outa = col ? k : n;
    const size_t lda = bmb_bench_ld(outa, col ? n : k, sizeof(float));
    const size_t ldc = bmb_bench_ld(n, n, sizeof(float));
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->alpha = 1.0f;
    ctx->beta = 0.0f; /* verify() relies on it */
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->ldc = (int) ldc;
    ctx->a = bmb_bench_alloc(outa * lda * sizeof(float));
    ctx->c = bmb_bench_alloc(n * ldc * sizeof(float));
    if (ctx->a == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < outa * lda; i++) {
        ctx->a[i] = (float) (i % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < n * ldc; i++) {
        ctx->c[i] = 0.0f;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_ssyrk(ctx->order, CblasUpper, CblasNoTrans, ctx->n, ctx->k,
                ctx->alpha, ctx->a, ctx->lda, ctx->beta, ctx->c, ctx->ldc);
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

/* --cache cold: the bytes of A and C: N x K, N x N. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return (d1 * d2 + d1 * d1) * (double) (sizeof(float));
}

/* --verify, by random projection in double: Sym(C)*r against
 * alpha*A*(A'*r), Sym(C) being the symmetric matrix C's upper triangle
 * stands for; beta is 0 (setup), so what C held before the call does not
 * enter. The scale leaves out |C||r|, as sgemm's does: the library's
 * error on C is bounded by the tolerance of its K terms times
 * |alpha||A||A'|. The strictly lower part, which the
 * routine must not touch, is projected before and after the call, and
 * must come out the same to the bit. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, k = (size_t) ctx->k, i;
    double *r = malloc((12 * n + 2 * k) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + n, *tabs = t + k;
    double *y1 = tabs + k, *y1abs = y1 + n, *y2 = y1abs + n, *y2abs = y2 + n;
    double *u = y2abs + n, *uabs = u + n, *l0 = uabs + n, *l = l0 + n, *labs = l + n;
    double *want = labs + n, *scale = want + n;

    (void) u;
    (void) uabs;
    bmb_verify_probe(r, n);
    bmb_verify_matvec_single(BMB_VERIFY_LOWER_STRICT, ctx->col, n, n, ctx->c, ctx->ldc, r, NULL, l0, labs);
    call(ctx);
    bmb_verify_perturb_single(&ctx->c[0]);
    bmb_verify_matvec_single(BMB_VERIFY_SYM_UPPER, ctx->col, n, n, ctx->c, ctx->ldc, r, NULL, y1, y1abs);
    bmb_verify_matvec_single(BMB_VERIFY_FULL_T, ctx->col, n, k, ctx->a, ctx->lda, r, NULL, t, tabs);
    bmb_verify_matvec_single(BMB_VERIFY_FULL, ctx->col, n, k, ctx->a, ctx->lda, t, tabs, y2, y2abs);
    for (i = 0; i < n; i++) {
        want[i] = ctx->alpha * y2[i];
        scale[i] = fabs(ctx->alpha) * y2abs[i];
    }
    ok = bmb_verify_close("C*r, for a random vector r", y1, want, scale, n,
                          bmb_verify_tolerance_single(k), msg, size);
    if (ok) {
        bmb_verify_matvec_single(BMB_VERIFY_LOWER_STRICT, ctx->col, n, n, ctx->c, ctx->ldc, r, NULL, l, labs);
        ok = bmb_verify_close("the strictly lower part of C, times r, which the call must not change",
                              l, l0, NULL, n, 0.0, msg, size);
    }
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "ssyrk";
    bench.dim1_label = "Matrix dim1 (N)";
    bench.dim2_label = "Matrix dim2 (K)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
