#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* ssymv and dsymv, one per precision (bmb_prec.h); the complex one,
 * hemv, is a family of its own.
 * Square routine: only dim1 (= N) is used, swept via --matrix-dim1. */
#if BMB_COMPLEX
#error "symv is real: the complex one is hemv"
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int n;
    int lda;
    char uplo; /* --uplo: the triangle A is stored in */
    bmb_real_t alpha;
    bmb_real_t beta;
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

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->lda = (int) lda;
    ctx->uplo = bmb_bench_uplo();
    ctx->alpha = BMB_R(1.0);
    ctx->beta = BMB_R(0.0);
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

    bmb_fill(ctx->a, n * lda, 0, 0);
    bmb_fill(ctx->x, n, 0, 0);
    for (i = 0; i < n; i++) {
        ctx->y[i] = BMB_R(0.0);
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(symv)(ctx->order, BMB_CBLAS_UPLO(ctx->uplo), ctx->n, ctx->alpha,
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

    (void) dim2;
    return 2.0 * d1 * d1;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (double) BMB_ELEM_BYTES * (d1 * (d1 + 1.0) / 2.0 + 3.0 * d1);
}

/* --verify: alpha * A * x + beta * y0, recomputed in double. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, i;
    double *y0 = malloc(7 * n * sizeof(double));
    const double alpha = ctx->alpha, beta = ctx->beta;
    int ok;

    if (y0 == NULL) {
        return -1;
    }
    double *t = y0 + n, *tabs = y0 + 2 * n, *want = y0 + 3 * n, *scale = y0 + 4 * n;
    double *got = y0 + 5 * n, *x = y0 + 6 * n;

    bmb_to_double(y0, ctx->y, n);
    bmb_to_double(x, ctx->x, n);
    call(ctx);
    bmb_perturb(&ctx->y[0]);
    bmb_matvec(ctx->uplo == 'U' ? BMB_VERIFY_SYM_UPPER : BMB_VERIFY_SYM_LOWER, ctx->col, n, n, ctx->a,
               (size_t) ctx->lda, x, NULL, t, tabs);
    bmb_to_double(got, ctx->y, n);
    for (i = 0; i < n; i++) {
        want[i] = alpha * t[i] + beta * y0[i];
        scale[i] = fabs(alpha) * tabs[i] + fabs(beta * y0[i]);
    }
    ok = bmb_verify_close("y", got, want, scale, n, bmb_tolerance(n), msg, size);
    free(y0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(symv);
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
