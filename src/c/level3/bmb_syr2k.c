#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* ssyr2k, dsyr2k, csyr2k and zsyr2k, one per precision (bmb_prec.h): C
 * (N x N, symmetric) = alpha * (A * B^T + B * A^T) + beta * C, A and B
 * are N x K. dim1 = N, dim2 = K. */

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldc;
    int n;
    int k;
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t beta[BMB_PARTS];
    bmb_real_t *a; /* N x K */
    bmb_real_t *b; /* N x K */
    bmb_real_t *c; /* N x N, upper triangle used */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1, k = dim2;
    const int col = bmb_bench_column_major();
    /* A as stored: its rows, or its columns with --layout col. */
    const size_t outa = col ? k : n;
    const size_t lda = bmb_bench_ld(outa, col ? n : k, BMB_ELEM_BYTES);
    const size_t ldc = bmb_bench_ld(n, n, BMB_ELEM_BYTES);
    size_t i;

    (void) thread_count;

#if BMB_ELEM_BYTES > 8
    if (lda == 0 || ldc == 0) {
        bmb_bench_too_large();
        return NULL;
    }
#endif
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->k = (int) k;
    bmb_set_scalar(ctx->alpha, BMB_R(1.0), BMB_R(0.0));
    bmb_set_scalar(ctx->beta, BMB_R(0.0), BMB_R(0.0)); /* verify() relies on it */
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->ldc = (int) ldc;
    ctx->a = bmb_bench_alloc(outa * lda * BMB_ELEM_BYTES);
    ctx->b = bmb_bench_alloc(outa * lda * BMB_ELEM_BYTES);
    ctx->c = bmb_bench_alloc(n * ldc * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, outa * lda, 0, 31);
    bmb_fill(ctx->b, outa * lda, 7, 53);
    for (i = 0; i < BMB_PARTS * n * ldc; i++) {
        ctx->c[i] = BMB_R(0.0);
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(syr2k)(ctx->order, CblasUpper, CblasNoTrans, ctx->n, ctx->k,
                     BMB_SCALAR(ctx->alpha), (const void *) ctx->a, ctx->lda,
                     (const void *) ctx->b, ctx->lda, BMB_SCALAR(ctx->beta),
                     (void *) ctx->c, ctx->ldc);
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

    return 2.0 * BMB_FLOP_FACTOR * d2 * d1 * (d1 + 1.0);
}

/* --cache cold: the bytes of A, B and C: N x K, N x K, N x N. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return (2.0 * d1 * d2 + d1 * d1) * (double) (BMB_ELEM_BYTES);
}

/* --verify, by random projection in double: C*r against
 * alpha*(A*(B^T*r) + B*(A^T*r)), r of the routine's kind, C standing for
 * the symmetric matrix its upper triangle gives; beta is 0 (setup), so
 * what C held before the call does not enter. In single precision, the
 * scale leaves out |C||r|, and the tolerance counts the library's 2K
 * terms alone, as for ssyrk. The strictly lower part, which the routine
 * must not touch, is projected before and after the call, and must come
 * out the same to the bit. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t n = (size_t) ctx->n, k = (size_t) ctx->k, i;
    double *r = malloc(((8 * p + 4) * n + (p + 1) * k) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + p * n, *tabs = t + p * k;
    double *y1 = tabs + k, *y1abs = y1 + p * n, *y2 = y1abs + n, *y2abs = y2 + p * n;
    double *u = y2abs + n, *uabs = u + p * n, *l0 = uabs + n, *l = l0 + p * n, *labs = l + p * n;
    double *want = labs + n, *scale = want + p * n;

    bmb_verify_probe(r, p * n);
    bmb_matvec(BMB_VERIFY_LOWER_STRICT, ctx->col, n, n, ctx->c, (size_t) ctx->ldc, r, NULL, l0, labs);
    call(ctx);
    bmb_perturb(&ctx->c[0]);
    bmb_matvec(BMB_VERIFY_SYM_UPPER, ctx->col, n, n, ctx->c, (size_t) ctx->ldc, r, NULL, y1, y1abs);
    bmb_matvec(BMB_VERIFY_FULL_T, ctx->col, n, k, ctx->b, (size_t) ctx->lda, r, NULL, t, tabs);
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, n, k, ctx->a, (size_t) ctx->lda, t, tabs, y2, y2abs);
    bmb_matvec(BMB_VERIFY_FULL_T, ctx->col, n, k, ctx->a, (size_t) ctx->lda, r, NULL, t, tabs);
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, n, k, ctx->b, (size_t) ctx->lda, t, tabs, u, uabs);
    for (i = 0; i < p * n; i++) {
        y2[i] += u[i];
    }
    for (i = 0; i < n; i++) {
        y2abs[i] += uabs[i];
    }
    bmb_scale(want, ctx->alpha, y2, n);
    for (i = 0; i < p * n; i++) {
        scale[i] = bmb_abs(ctx->alpha) * y2abs[i / p];
#if !BMB_SINGLE
        scale[i] += y1abs[i / p];
#endif
    }
    ok = bmb_verify_close("C*r, for a random vector r" BMB_PARTS_NOTE, y1, want, scale, p * n,
                          bmb_tolerance(BMB_SINGLE ? 2 * k : k + n), msg, size);
    if (ok) {
        bmb_matvec(BMB_VERIFY_LOWER_STRICT, ctx->col, n, n, ctx->c, (size_t) ctx->ldc, r, NULL, l, labs);
        ok = bmb_verify_close("the strictly lower part of C, times r, which the call must not change",
                              l, l0, NULL, p * n, 0.0, msg, size);
    }
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(syr2k);
    bench.dim1_label = "Matrix dim1 (N)";
    bench.dim2_label = "Matrix dim2 (K)";
    bench.use_vector_range = 0;
    bench.level = 3;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
