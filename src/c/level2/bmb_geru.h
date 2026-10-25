/* The source of geru and of gerc, which bmb_geru.c and bmb_gerc.c
 * compile, the second with BMB_CONJ defined (#236): a header, so that
 * the two programs share it without one source including another. */
#ifndef BMB_GERU_H
#define BMB_GERU_H

#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* cgeru and zgeru, one per precision (bmb_prec.h): A += alpha x y^T; with
 * BMB_CONJ defined, cgerc and zgerc (bmb_gerc.c), A += alpha x y^H. The
 * complex rank-1 updates; the real one, ger, is a family of its own.
 * dim1 = M, dim2 = N, A M x N updated in place. */
#if !BMB_COMPLEX
#error "geru and gerc are complex: the real one is ger"
#endif

#ifdef BMB_CONJ
#define BMB_OP gerc
/* y's imaginary part as the update takes it */
#define BMB_YI(yi) (-(yi))
#else
#define BMB_OP geru
#define BMB_YI(yi) (yi)
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int m;
    int n;
    bmb_real_t alpha[2];
    bmb_real_t *a;
    bmb_real_t *x;
    bmb_real_t *y;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    const int col = bmb_bench_column_major();
    /* A as stored: its rows, or its columns with --layout col. */
    const size_t outer = col ? dim2 : dim1;
    const size_t lda = bmb_bench_ld(outer, col ? dim1 : dim2, BMB_ELEM_BYTES);

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

    ctx->m = (int) dim1;
    ctx->n = (int) dim2;
    /* Small alpha: A accumulates rank-1 updates across iterations, kept
     * bounded regardless of --iterations. */
    bmb_set_scalar(ctx->alpha, BMB_R(1.0e-6), BMB_R(1.0e-6));
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->a = bmb_bench_alloc(outer * lda * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    ctx->y = bmb_bench_alloc(dim2 * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->x == NULL || ctx->y == NULL) {
        free(ctx->a);
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, outer * lda, 0, 31);
    bmb_fill(ctx->x, dim1, 0, 37);
    bmb_fill(ctx->y, dim2, 7, 53);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(BMB_OP)(ctx->order, ctx->m, ctx->n, (const void *) ctx->alpha, (const void *) ctx->x, 1,
                      (const void *) ctx->y, 1, (void *) ctx->a, ctx->lda);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

/* A complex multiply-add for each of the M x N elements. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return BMB_MADD_FLOPS * d1 * d2;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return (double) BMB_ELEM_BYTES * (2.0 * d1 * d2 + d1 + d2);
}

/* --verify, by random projection in double: A*r against
 * A0*r + alpha*x*(y^T r), y^H r for gerc. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, i, j;
    double *r = malloc((2 * n + 10 * m) * sizeof(double));
    const double ar = ctx->alpha[0], ai = ctx->alpha[1];
    double sr = 0.0, si = 0.0, sabs = 0.0;
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *p0 = r + 2 * n, *p0abs = p0 + 2 * m, *p = p0abs + m, *pabs = p + 2 * m;
    double *want = pabs + m, *scale = want + 2 * m;

    bmb_verify_probe(r, 2 * n);
    for (j = 0; j < n; j++) {
        const double yr = ctx->y[2 * j], yi = BMB_YI(ctx->y[2 * j + 1]);
        const double rr = r[2 * j], ri = r[2 * j + 1];

        sr += yr * rr - yi * ri;
        si += yr * ri + yi * rr;
        sabs += (fabs(yr) + fabs(yi)) * (fabs(rr) + fabs(ri));
    }
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->a, (size_t) ctx->lda, r, NULL, p0, p0abs);
    call(ctx);
    bmb_perturb(&ctx->a[0]);
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->a, (size_t) ctx->lda, r, NULL, p, pabs);
    /* alpha * s, then times x_i */
    {
        const double tr = ar * sr - ai * si, ti = ar * si + ai * sr;

        for (i = 0; i < m; i++) {
            const double xr = ctx->x[2 * i], xi = ctx->x[2 * i + 1];

            want[2 * i] = p0[2 * i] + (xr * tr - xi * ti);
            want[2 * i + 1] = p0[2 * i + 1] + (xr * ti + xi * tr);
            scale[2 * i] = scale[2 * i + 1] = pabs[i] + p0abs[i]
                                              + (fabs(ar) + fabs(ai)) * (fabs(xr) + fabs(xi)) * sabs;
        }
    }
    ok = bmb_verify_close("A*r, for a random vector r" BMB_PARTS_NOTE, p, want, scale, 2 * m,
                          bmb_tolerance(n), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(BMB_OP);
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.level = 2;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}

#endif /* BMB_GERU_H */
