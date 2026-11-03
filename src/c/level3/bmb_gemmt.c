#include <cblas.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sgemmt, dgemmt, cgemmt and zgemmt, one per precision (bmb_prec.h): C (N
 * x N) = alpha * op(A) * op(B) + beta * C in the triangle --uplo gives,
 * op(A) N x K, op(B) K x N, as --trans gives them. An extension of the
 * BLAS (#171) that MKL, NVPL, AOCL-BLAS, BLIS, ArmPL and OpenBLAS (from
 * 0.3.28) have; a build whose library does not is skipped. dim1 = N,
 * dim2 = K. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    bmb_int lda;
    bmb_int ldb;
    bmb_int ldc;
    bmb_int n;
    bmb_int k;
    char uplo, ta, tb;
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t beta[BMB_PARTS];
    size_t sa, sb; /* A's and B's elements, leading dimensions in */
    bmb_real_t *a; /* op(A) is N x K */
    bmb_real_t *b; /* op(B) is K x N */
    bmb_real_t *c; /* N x N, the triangle --uplo gives written */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1, k = dim2;
    const int col = bmb_bench_column_major();
    const char ta = bmb_bench_trans(0), tb = bmb_bench_trans(1);
    /* A is ra x ca, B rb x cb, each stored as its rows, or its columns
     * with --layout col. */
    const size_t ra = ta == 'N' ? n : k, ca = ta == 'N' ? k : n;
    const size_t rb = tb == 'N' ? k : n, cb = tb == 'N' ? n : k;
    const size_t outa = col ? ca : ra, outb = col ? cb : rb;
    const size_t lda = bmb_bench_ld(outa, col ? ra : ca, BMB_ELEM_BYTES);
    const size_t ldb = bmb_bench_ld(outb, col ? rb : cb, BMB_ELEM_BYTES);
    const size_t ldc = bmb_bench_ld(n, n, BMB_ELEM_BYTES);
    size_t i;

    (void) thread_count;

#if BMB_ELEM_BYTES > 8
    if (lda == 0 || ldb == 0 || ldc == 0) {
        bmb_bench_too_large();
        return NULL;
    }
#endif
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (bmb_int) n;
    ctx->k = (bmb_int) k;
    ctx->uplo = bmb_bench_uplo();
    ctx->ta = ta;
    ctx->tb = tb;
    bmb_set_scalar(ctx->alpha, BMB_R(1.0), BMB_R(0.0));
    bmb_set_scalar(ctx->beta, BMB_R(0.0), BMB_R(0.0)); /* verify() relies on it */
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (bmb_int) lda;
    ctx->ldb = (bmb_int) ldb;
    ctx->ldc = (bmb_int) ldc;
    ctx->sa = outa * lda;
    ctx->sb = outb * ldb;
    ctx->a = bmb_bench_alloc(outa * lda * BMB_ELEM_BYTES);
    ctx->b = bmb_bench_alloc(outb * ldb * BMB_ELEM_BYTES);
    ctx->c = bmb_bench_alloc(n * ldc * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, outa * lda, 0, 31);
    bmb_fill(ctx->b, outb * ldb, 7, 53);
    for (i = 0; i < BMB_PARTS * n * ldc; i++) {
        ctx->c[i] = BMB_R(0.0);
    }

    return ctx;
}

/* gemmtr where the library has it: the Reference BLAS's name for it from
 * LAPACK 3.12, and MKL's, which deprecates gemmt (to go in 2029). */
#if defined(HAVE_CBLAS_GEMMTR)
#define BMB_GEMMT_CALL gemmtr
#else
#define BMB_GEMMT_CALL gemmt
#endif

static void call(void *vctx)
{
#if defined(HAVE_CBLAS_GEMMT)
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(BMB_GEMMT_CALL)(ctx->order, BMB_CBLAS_UPLO(ctx->uplo), BMB_CBLAS_TRANS(ctx->ta),
                     BMB_CBLAS_TRANS(ctx->tb), ctx->n, ctx->k, BMB_SCALAR(ctx->alpha),
                     (const void *) ctx->a, ctx->lda, (const void *) ctx->b, ctx->ldb,
                     BMB_SCALAR(ctx->beta), (void *) ctx->c, ctx->ldc);
#else
    (void) vctx;
#endif
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b);
    free(ctx->c);
    free(ctx);
}

/* N (N + 1) / 2 elements of K multiply-adds: half a GEMM and its
 * diagonal, as syrk's. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return BMB_FLOP_FACTOR * d2 * d1 * (d1 + 1.0);
}

/* --cache cold: the bytes of A, B and C: N x K, K x N, N x N. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return (2.0 * d1 * d2 + d1 * d1) * (double) (BMB_ELEM_BYTES);
}

/* Element (i, j) of op(X), X stored as the benchmark stores it, in
 * double: re, and im (0 for a real one). */
static void op_at(const bmb_real_t *x, int col, size_t ld, char op, size_t i, size_t j,
                  double *re, double *im)
{
    const size_t r = op == 'N' ? i : j, c = op == 'N' ? j : i;
    const size_t at = col ? c * ld + r : r * ld + c;

    *re = (double) x[BMB_PARTS * at];
#if BMB_COMPLEX
    *im = (double) x[BMB_PARTS * at + 1];
    if (op == 'C') {
        *im = -*im;
    }
#else
    *im = 0.0;
#endif
}

/* --verify, by random projection in double: the triangle of C, times r,
 * against that of op(A) * op(B), times r, alpha applied; beta is 0
 * (setup), so what C held before does not enter. The triangle of a
 * product is no product of triangles, so its projection is made in
 * O(N K) rather than O(N^2 K): row i of the upper one is op(A)'s row i
 * times s_i, the sum over j >= i of op(B)'s column j times r_j, which
 * grows by one column a row, from the last row up (the lower one, from
 * the first down). The other triangle's strict part, which the call must
 * not touch, is projected before and after it and must come out the same
 * to the bit; so must A and B, which it only reads: OpenBLAS before 0.3.30
 * conjugated B in place where op(B) is B^H (OpenMathLib/OpenBLAS#5143),
 * and said so only as a wrong C. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t n = (size_t) ctx->n, k = (size_t) ctx->k, i, l, step;
    const int upper = ctx->uplo == 'U';
    const int strict = upper ? BMB_VERIFY_LOWER_STRICT : BMB_VERIFY_UPPER_STRICT;
    double *r = malloc(((7 * p + 3) * n + (2 * p + 1) * k) * sizeof(double));
    bmb_real_t *a0 = malloc((ctx->sa + ctx->sb) * BMB_ELEM_BYTES);
    int ok;

    if (r == NULL || a0 == NULL) {
        free(r);
        free(a0);
        return -1;
    }
    bmb_real_t *b0 = a0 + BMB_PARTS * ctx->sa;
    double *y1 = r + p * n, *y1abs = y1 + p * n, *y2 = y1abs + n, *y2abs = y2 + p * n;
    double *l0 = y2abs + n, *lp = l0 + p * n, *labs = lp + p * n;
    double *want = labs + n, *scale = want + p * n;
    double *s = scale + p * n, *sabs = s + p * k;

    memcpy(a0, ctx->a, ctx->sa * BMB_ELEM_BYTES);
    memcpy(b0, ctx->b, ctx->sb * BMB_ELEM_BYTES);
    bmb_verify_probe(r, p * n);
    bmb_matvec(strict, ctx->col, n, n, ctx->c, (size_t) ctx->ldc, r, NULL, l0, labs);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    if (memcmp(a0, ctx->a, ctx->sa * BMB_ELEM_BYTES) != 0 || memcmp(b0, ctx->b, ctx->sb * BMB_ELEM_BYTES) != 0) { /* GCOVR_EXCL_BR_LINE: only a library that changes A or B, OpenBLAS before 0.3.30; the coverage jobs have none that does (Ubuntu's OpenBLAS has no gemmt, BLIS's leaves A and B alone) */
        /* GCOVR_EXCL_START: only a library that changes A or B, OpenBLAS before 0.3.30; the coverage jobs have none that does (Ubuntu's OpenBLAS has no gemmt, BLIS's leaves A and B alone) */
        snprintf(msg, size, "the call changed %s, which it must only read",
                 memcmp(a0, ctx->a, ctx->sa * BMB_ELEM_BYTES) != 0 ? "A" : "B");
        free(r);
        free(a0);
        return 1;
        /* GCOVR_EXCL_STOP */
    }
    bmb_perturb(&ctx->c[0]);
    bmb_matvec(upper ? BMB_VERIFY_UPPER : BMB_VERIFY_LOWER, ctx->col, n, n, ctx->c,
               (size_t) ctx->ldc, r, NULL, y1, y1abs);

    for (l = 0; l < p * k; l++) {
        s[l] = 0.0;
    }
    for (l = 0; l < k; l++) {
        sabs[l] = 0.0;
    }
    for (step = 0; step < n; step++) {
        const size_t row = upper ? n - 1 - step : step;
        const double rr = r[p * row], ri = BMB_COMPLEX ? r[p * row + 1] : 0.0;
        double yr = 0.0, yi = 0.0, ya = 0.0;

        for (l = 0; l < k; l++) {
            double br, bi;

            op_at(ctx->b, ctx->col, (size_t) ctx->ldb, ctx->tb, l, row, &br, &bi);
            s[p * l] += br * rr - bi * ri;
#if BMB_COMPLEX
            s[p * l + 1] += br * ri + bi * rr;
#endif
            sabs[l] += (fabs(br) + fabs(bi)) * (fabs(rr) + fabs(ri));
        }
        for (l = 0; l < k; l++) {
            double ar, ai;
            const double sr = s[p * l], si = BMB_COMPLEX ? s[p * l + 1] : 0.0;

            op_at(ctx->a, ctx->col, (size_t) ctx->lda, ctx->ta, row, l, &ar, &ai);
            yr += ar * sr - ai * si;
            yi += ar * si + ai * sr;
            ya += (fabs(ar) + fabs(ai)) * sabs[l];
        }
        y2[p * row] = yr;
#if BMB_COMPLEX
        y2[p * row + 1] = yi;
#else
        (void) yi;
#endif
        y2abs[row] = ya;
    }
    bmb_scale(want, ctx->alpha, y2, n);
    for (i = 0; i < p * n; i++) {
        scale[i] = bmb_abs(ctx->alpha) * y2abs[i / p];
#if !BMB_SINGLE
        scale[i] += y1abs[i / p];
#endif
    }
    ok = bmb_verify_close(upper ? "C's upper triangle times r, for a random vector r" BMB_PARTS_NOTE
                                : "C's lower triangle times r, for a random vector r" BMB_PARTS_NOTE,
                          y1, want, scale, p * n, bmb_tolerance(BMB_SINGLE ? k : k + n), msg, size);
    if (ok) {
        bmb_matvec(strict, ctx->col, n, n, ctx->c, (size_t) ctx->ldc, r, NULL, lp, labs);
        ok = bmb_verify_close(upper
                                  ? "the strictly lower part of C, times r, which the call must not change"
                                  : "the strictly upper part of C, times r, which the call must not change",
                              lp, l0, NULL, p * n, 0.0, msg, size);
    }
    free(r);
    free(a0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(gemmt);
    bench.dim1_label = "Matrix dim1 (N)";
    bench.dim2_label = "Matrix dim2 (K)";
    bench.use_vector_range = 0;
    bench.level = 3;
    bench.args = BMB_ARG_UPLO | BMB_ARG_TRANS2;
    bench.trans_letters = BMB_TRANS_LETTERS;
#if !defined(HAVE_CBLAS_GEMMT)
    bench.missing = "cblas_?gemmt or cblas_?gemmtr";
#endif
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
