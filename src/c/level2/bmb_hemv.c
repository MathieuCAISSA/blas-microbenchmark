#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* chemv and zhemv, one per precision (bmb_prec.h): y = alpha A x + beta y,
 * A Hermitian, stored in the triangle --uplo gives; the real one, symv,
 * is a family of its own. Square routine: only dim1 (= N) is used. A's
 * diagonal has an imaginary part, which a Hermitian matrix cannot have
 * and BLAS does not read: --verify ignores it too, so a hemv that used it
 * would be caught. */
#if !BMB_COMPLEX
#error "hemv is complex: the real one is symv"
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int n;
    int lda;
    char uplo; /* --uplo: the triangle A is stored in */
    bmb_real_t alpha[2];
    bmb_real_t beta[2];
    bmb_real_t *a; /* N x N, in the order of --layout, the triangle --uplo gives used */
    bmb_real_t *x; /* N */
    bmb_real_t *y; /* N */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1;
    const size_t lda = bmb_bench_ld(n, n, BMB_ELEM_BYTES);
    size_t i;

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
    /* beta 0, as symv's: y is overwritten, and stays bounded over calls. */
    bmb_set_scalar(ctx->alpha, BMB_R(1.0), BMB_R(0.25));
    bmb_set_scalar(ctx->beta, BMB_R(0.0), BMB_R(0.0));
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
    for (i = 0; i < 2 * n; i++) {
        ctx->y[i] = BMB_R(0.0);
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(hemv)(ctx->order, BMB_CBLAS_UPLO(ctx->uplo), ctx->n, (const void *) ctx->alpha,
                    (const void *) ctx->a, ctx->lda, (const void *) ctx->x, 1, (const void *) ctx->beta,
                    (void *) ctx->y, 1);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

/* symv's 2 N^2, in complex multiply-adds. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return BMB_MADD_FLOPS * d1 * d1;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (double) BMB_ELEM_BYTES * (d1 * (d1 + 1.0) / 2.0 + 3.0 * d1);
}

/* --verify: alpha * A * x + beta * y0, recomputed in double, A read as the
 * Hermitian matrix its triangle stands for. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, i;
    double *y0 = malloc(13 * n * sizeof(double));
    const double br = ctx->beta[0], bi = ctx->beta[1];
    int ok;

    if (y0 == NULL) {
        return -1;
    }
    double *t = y0 + 2 * n, *tabs = t + 2 * n, *want = tabs + n, *scale = want + 2 * n;
    double *got = scale + 2 * n, *x = got + 2 * n;

    bmb_to_double(y0, ctx->y, n);
    bmb_to_double(x, ctx->x, n);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->y[0]);
    bmb_matvec(ctx->uplo == 'U' ? BMB_VERIFY_HERM_UPPER : BMB_VERIFY_HERM_LOWER, ctx->col, n, n, ctx->a,
               (size_t) ctx->lda, x, NULL, t, tabs);
    bmb_to_double(got, ctx->y, n);
    bmb_scale(want, ctx->alpha, t, n);
    for (i = 0; i < n; i++) {
        const double yr = y0[2 * i], yi = y0[2 * i + 1];

        want[2 * i] += br * yr - bi * yi;
        want[2 * i + 1] += br * yi + bi * yr;
        scale[2 * i] = scale[2 * i + 1] = bmb_abs(ctx->alpha) * tabs[i]
                                          + bmb_abs(ctx->beta) * (fabs(yr) + fabs(yi));
    }
    ok = bmb_verify_close("y" BMB_PARTS_NOTE, got, want, scale, 2 * n, bmb_tolerance(n), msg, size);
    free(y0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(hemv);
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
