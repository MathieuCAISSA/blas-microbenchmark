#include <cblas.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_log.h"
#include "bmb_prec.h"
#include "bmb_verify.h"
#include "bmb_band.h"

/* sgbmv and dgbmv, one per precision (bmb_prec.h): y = alpha * op(A) * x,
 * op as --trans gives it, A N x N banded (bmb_band.h): K diagonals below
 * the main one and K above. dim1 = N, dim2 = K. A K of N - 1 or more
 * makes the band reach past the matrix's corners, which BLAS takes: the
 * whole matrix is then computed on, and the counts say so. The complex
 * ones are not here yet. */
#if BMB_COMPLEX
#error "gbmv is real here: cgbmv and zgbmv are not here yet"
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;   /* 2K + 1, and --ld-pad */
    int n;
    int k;
    char trans; /* --trans: N or T */
    bmb_real_t alpha;
    bmb_real_t beta;
    bmb_real_t *a; /* the band, N columns (rows with --layout row) of lda */
    bmb_real_t *x; /* N */
    bmb_real_t *y; /* N */
} bmb_ctx_t;

/* The band's elements inside the matrix: all N^2 once K reaches N - 1. */
static double bmb_band_elements(double n, double k)
{
    return k >= n - 1.0 ? n * n : n * (2.0 * k + 1.0) - k * (k + 1.0);
}

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1, k = dim2, lda, i;

    (void) thread_count;

    lda = bmb_bench_ld(n, 2 * k + 1, BMB_ELEM_BYTES);
    if (lda == 0) {
        bmb_bench_too_large();
        return NULL;
    }
    /* The parser keeps K and --ld-pad within an int, but not 2K + 1. */
    if (lda > (size_t) INT_MAX) {
        bmb_log_error("A band of 2K + 1 diagonals would not fit the int BLAS takes: K is too large.");
        return NULL;
    }
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->lda = (int) lda;
    ctx->trans = bmb_bench_trans(0);
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

    /* The whole array, the corners the band does not reach included:
     * values a library must not read, which --verify does not either. */
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

    BMB_CBLAS(gbmv)(ctx->order, BMB_CBLAS_TRANS(ctx->trans), ctx->n, ctx->n, ctx->k, ctx->k, ctx->alpha,
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

/* gemv's 2 a matrix element, over the band's elements inside the matrix. */
static double flops(size_t dim1, size_t dim2)
{
    return 2.0 * bmb_band_elements((double) dim1, (double) dim2);
}

/* Those elements read once, x read once, y read and written. */
static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    return (double) BMB_ELEM_BYTES * (bmb_band_elements(d1, (double) dim2) + 3.0 * d1);
}

/* --verify: alpha * op(A) * x + beta * y0, recomputed in double, A read
 * straight from the band (bmb_band_matvec). */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, k = (size_t) ctx->k, i;
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
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->y[0]);
    bmb_band_matvec(n, n, k, k, ctx->trans, 0, ctx->a, (size_t) ctx->lda, ctx->col, x, t, tabs);
    bmb_to_double(got, ctx->y, n);
    for (i = 0; i < n; i++) {
        want[i] = alpha * t[i] + beta * y0[i];
        scale[i] = fabs(alpha) * tabs[i] + fabs(beta * y0[i]);
    }
    ok = bmb_verify_close("y", got, want, scale, n, bmb_tolerance(2 * k + 1 < n ? 2 * k + 1 : n), msg,
                          size);
    free(y0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(gbmv);
    bench.dim1_label = "Matrix dim1 (N)";
    bench.dim2_label = "Matrix dim2 (K)";
    bench.use_vector_range = 0;
    bench.level = 2;
    bench.args = BMB_ARG_TRANS;
    bench.trans_letters = "NT";
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
