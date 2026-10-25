#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* cher and zher, one per precision (bmb_prec.h): A += alpha x x^H, alpha
 * real, A Hermitian, the triangle --uplo gives updated in place; the real
 * one, syr, is a family of its own. Square routine: only dim1 (= N) is
 * used. BLAS sets the imaginary parts of A's diagonal to zero, a
 * Hermitian matrix's being real; --verify reads A as the Hermitian matrix
 * its triangle stands for, which ignores them. */
#if !BMB_COMPLEX
#error "her is complex: the real one is syr"
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int n;
    int lda;
    char uplo; /* --uplo: the triangle updated */
    bmb_real_t alpha;
    bmb_real_t *a;
    bmb_real_t *x;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1;
    const size_t lda = bmb_bench_ld(n, n, BMB_ELEM_BYTES);

    (void) dim2;
    (void) thread_count;

#if BMB_ELEM_BYTES > 8
    if (lda == 0) {
        bmb_bench_too_large();
        return NULL;
    }
#endif
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->lda = (int) lda;
    ctx->uplo = bmb_bench_uplo();
    /* Small alpha: A accumulates rank-1 updates across iterations. */
    ctx->alpha = BMB_R(1.0e-6);
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->a = bmb_bench_alloc(n * lda * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(n * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->x == NULL) {
        free(ctx->a);
        free(ctx->x);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, n * lda, 0, 31);
    bmb_fill(ctx->x, n, 0, 37);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(her)(ctx->order, BMB_CBLAS_UPLO(ctx->uplo), ctx->n, ctx->alpha, (const void *) ctx->x, 1,
                   (void *) ctx->a, ctx->lda);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->x);
    free(ctx);
}

/* syr's N (N + 1), in complex multiply-adds. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return BMB_FLOP_FACTOR * d1 * (d1 + 1.0);
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (double) BMB_ELEM_BYTES * (d1 * (d1 + 1.0) + d1);
}

/* --verify, by random projection in double: A*r against
 * A0*r + alpha x (x^H r), A read as the Hermitian matrix its triangle
 * stands for, whose update is Hermitian too; and the other triangle,
 * which that reading does not see, unchanged. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, i;
    const int shape = ctx->uplo == 'U' ? BMB_VERIFY_HERM_UPPER : BMB_VERIFY_HERM_LOWER;
    const int strict = ctx->uplo == 'U' ? BMB_VERIFY_LOWER_STRICT : BMB_VERIFY_UPPER_STRICT;
    double *r = malloc(17 * n * sizeof(double));
    const double alpha = ctx->alpha;
    double sr = 0.0, si = 0.0, sabs = 0.0;
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *p0 = r + 2 * n, *p0abs = p0 + 2 * n, *p = p0abs + n, *pabs = p + 2 * n;
    double *want = pabs + n, *scale = want + 2 * n, *l0 = scale + 2 * n, *l = l0 + 2 * n;
    double *labs = l + 2 * n;

    bmb_verify_probe(r, 2 * n);
    for (i = 0; i < n; i++) {
        /* conj(x_i) r_i */
        const double xr = ctx->x[2 * i], xi = -ctx->x[2 * i + 1];
        const double rr = r[2 * i], ri = r[2 * i + 1];

        sr += xr * rr - xi * ri;
        si += xr * ri + xi * rr;
        sabs += (fabs(xr) + fabs(xi)) * (fabs(rr) + fabs(ri));
    }
    bmb_matvec(shape, ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, p0, p0abs);
    bmb_matvec(strict, ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, l0, labs);
    call(ctx);
    bmb_perturb(&ctx->a[0]);
    bmb_matvec(shape, ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, p, pabs);
    for (i = 0; i < n; i++) {
        const double xr = ctx->x[2 * i], xi = ctx->x[2 * i + 1];

        want[2 * i] = p0[2 * i] + alpha * (xr * sr - xi * si);
        want[2 * i + 1] = p0[2 * i + 1] + alpha * (xr * si + xi * sr);
        scale[2 * i] = scale[2 * i + 1] = pabs[i] + p0abs[i] + fabs(alpha) * (fabs(xr) + fabs(xi)) * sabs;
    }
    ok = bmb_verify_close("A*r, for a random vector r" BMB_PARTS_NOTE, p, want, scale, 2 * n,
                          bmb_tolerance(n), msg, size);
    if (ok) {
        bmb_matvec(strict, ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, l, labs);
        ok = bmb_verify_close(ctx->uplo == 'U'
                                  ? "the strictly lower part of A, times r, which the call must not change"
                                  : "the strictly upper part of A, times r, which the call must not change",
                              l, l0, NULL, 2 * n, 0.0, msg, size);
    }
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(her);
    bench.dim1_label = "Matrix dim (N)";
    bench.dim2_label = NULL;
    bench.use_vector_range = 0;
    bench.level = 2;
    bench.args = BMB_ARG_UPLO;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
