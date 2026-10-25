#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* cher2 and zher2, one per precision (bmb_prec.h): A += alpha x y^H +
 * conj(alpha) y x^H, A Hermitian, the triangle --uplo gives updated in
 * place; the real one, syr2, is a family of its own. Square routine: only dim1 (= N) is
 * used. BLAS sets the imaginary parts of A's diagonal to zero, a
 * Hermitian matrix's being real; --verify reads A as the Hermitian matrix
 * its triangle stands for, which ignores them. */
#if !BMB_COMPLEX
#error "her2 is complex: the real one is syr2"
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int n;
    int lda;
    char uplo; /* --uplo: the triangle updated */
    bmb_real_t alpha[2];
    bmb_real_t *a;
    bmb_real_t *x;
    bmb_real_t *y;
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
    bmb_set_scalar(ctx->alpha, BMB_R(1.0e-6), BMB_R(1.0e-6));
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->a = bmb_bench_alloc(n * lda * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(n * BMB_ELEM_BYTES);
    ctx->y = bmb_bench_alloc(n * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->x == NULL || ctx->y == NULL) {
        free(ctx->a);
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, n * lda, 0, 31);
    bmb_fill(ctx->x, n, 0, 37);
    bmb_fill(ctx->y, n, 7, 53);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(her2)(ctx->order, BMB_CBLAS_UPLO(ctx->uplo), ctx->n, (const void *) ctx->alpha,
                    (const void *) ctx->x, 1, (const void *) ctx->y, 1, (void *) ctx->a, ctx->lda);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

/* syr2's 2 N (N + 1), in complex multiply-adds. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return BMB_MADD_FLOPS * d1 * (d1 + 1.0);
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (double) BMB_ELEM_BYTES * (d1 * (d1 + 1.0) + 2.0 * d1);
}

/* --verify, by random projection in double: A*r against A0*r +
 * alpha x (y^H r) + conj(alpha) y (x^H r), A read as the Hermitian
 * matrix its triangle stands for, whose update is Hermitian too; and the
 * other triangle, which that reading does not see, unchanged. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, i;
    const int shape = ctx->uplo == 'U' ? BMB_VERIFY_HERM_UPPER : BMB_VERIFY_HERM_LOWER;
    const int strict = ctx->uplo == 'U' ? BMB_VERIFY_LOWER_STRICT : BMB_VERIFY_UPPER_STRICT;
    double *r = malloc(17 * n * sizeof(double));
    const double ar = ctx->alpha[0], ai = ctx->alpha[1];
    double s[2] = { 0.0, 0.0 }, t[2] = { 0.0, 0.0 }, sabs = 0.0, tabs = 0.0;
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *p0 = r + 2 * n, *p0abs = p0 + 2 * n, *p = p0abs + n, *pabs = p + 2 * n;
    double *want = pabs + n, *scale = want + 2 * n, *l0 = scale + 2 * n, *l = l0 + 2 * n;
    double *labs = l + 2 * n;

    bmb_verify_probe(r, 2 * n);
    for (i = 0; i < n; i++) {
        /* s = y^H r, t = x^H r */
        const double xr = ctx->x[2 * i], xi = ctx->x[2 * i + 1];
        const double yr = ctx->y[2 * i], yi = ctx->y[2 * i + 1];
        const double rr = r[2 * i], ri = r[2 * i + 1], rabs = fabs(rr) + fabs(ri);

        s[0] += yr * rr + yi * ri;
        s[1] += yr * ri - yi * rr;
        t[0] += xr * rr + xi * ri;
        t[1] += xr * ri - xi * rr;
        sabs += (fabs(yr) + fabs(yi)) * rabs;
        tabs += (fabs(xr) + fabs(xi)) * rabs;
    }
    bmb_matvec(shape, ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, p0, p0abs);
    bmb_matvec(strict, ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, l0, labs);
    call(ctx);
    bmb_perturb(&ctx->a[0]);
    bmb_matvec(shape, ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, p, pabs);
    {
        /* alpha s and conj(alpha) t */
        const double as[2] = { ar * s[0] - ai * s[1], ar * s[1] + ai * s[0] };
        const double ct[2] = { ar * t[0] + ai * t[1], ar * t[1] - ai * t[0] };
        const double aabs = fabs(ar) + fabs(ai);

        for (i = 0; i < n; i++) {
            const double xr = ctx->x[2 * i], xi = ctx->x[2 * i + 1];
            const double yr = ctx->y[2 * i], yi = ctx->y[2 * i + 1];

            want[2 * i] = p0[2 * i] + (xr * as[0] - xi * as[1]) + (yr * ct[0] - yi * ct[1]);
            want[2 * i + 1] = p0[2 * i + 1] + (xr * as[1] + xi * as[0]) + (yr * ct[1] + yi * ct[0]);
            scale[2 * i] = scale[2 * i + 1] = pabs[i] + p0abs[i]
                                              + aabs * ((fabs(xr) + fabs(xi)) * sabs
                                                        + (fabs(yr) + fabs(yi)) * tabs);
        }
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

    bench.routine_name = BMB_ROUTINE(her2);
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
