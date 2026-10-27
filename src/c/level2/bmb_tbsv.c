#include <cblas.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"
#include "bmb_band.h"

/* stbsv and dtbsv, one per precision (bmb_prec.h): trsv on a triangular
 * A banded (bmb_band.h), K diagonals off the main one in the triangle
 * --uplo gives. dim1 = N, dim2 = K; a K of N - 1 or more is the whole
 * triangle, which BLAS takes. X is overwritten in place by the call; it
 * is restored from x0 before every call so the benchmark is stable
 * across --iterations. The complex ones, ctbsv and ztbsv, are not here
 * yet. */
#if BMB_COMPLEX
#error "tbsv is real here: ctbsv and ztbsv are not here yet"
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int n;
    int k;
    int lda; /* K + 1, and --ld-pad */
    char uplo, trans, diag; /* --uplo, --trans, --diag */
    bmb_real_t *a; /* the band, N columns (rows with --layout row) of lda */
    bmb_real_t *x0; /* template */
    bmb_real_t *x;  /* working buffer, restored by reset() before each call */
} bmb_ctx_t;

/* Off the diagonal, small enough to keep the matrix diagonally dominant
 * (at most 0.5 / n, each part of a complex element half that), and
 * neither symmetric nor Hermitian: with a constant there, the upper
 * triangle read row-major equals the one read column-major, and --verify
 * could not tell a library that ignored --layout, or conjugated A. */
static bmb_real_t bmb_off_diagonal(size_t i, size_t j, size_t n)
{
    return (bmb_real_t) ((i * 7 + j * 3) % 11 + 1) / (BMB_R(24.0) * (bmb_real_t) n);
}

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1, k = dim2;
    const size_t lda = bmb_bench_ld(n, k + 1, BMB_ELEM_BYTES);
    size_t i;

    (void) thread_count;

    if (lda == 0) {
        bmb_bench_too_large();
        return NULL;
    }
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->lda = (int) lda;
    ctx->uplo = bmb_bench_uplo();
    ctx->trans = bmb_bench_trans(0);
    ctx->diag = bmb_bench_diag();
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->a = bmb_bench_alloc(n * lda * BMB_ELEM_BYTES);
    ctx->x0 = bmb_bench_alloc(n * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(n * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->x0 == NULL || ctx->x == NULL) {
        free(ctx->a);
        free(ctx->x0);
        free(ctx->x);
        free(ctx);
        return NULL;
    }

    /* The whole array first: the corners the band does not reach hold
     * values a library must not read, which --verify does not either. */
    bmb_fill(ctx->a, n * lda, 0, 0);
    for (i = 0; i < n; i++) {
        const size_t lo = ctx->uplo == 'U' ? i : (i > k ? i - k : 0);
        const size_t hi = ctx->uplo == 'U' ? (k < n - i ? i + k + 1 : n) : i + 1;
        size_t j;

        for (j = lo; j < hi; j++) {
            /* Diagonally dominant so the triangular solve is well-conditioned. */
            ctx->a[bmb_band_index(i, j, ctx->uplo == 'U' ? 0 : k, ctx->uplo == 'U' ? k : 0, lda, ctx->col)] =
                (i == j) ? (bmb_real_t) (n + 1) : bmb_off_diagonal(i, j, n);
        }
        /* Away from zero, so that the solution stays of the size of x. */
        ctx->x0[i] = (bmb_real_t) (i % 100) * BMB_R(0.01) + BMB_R(1.0);
    }
    memcpy(ctx->x, ctx->x0, n * BMB_ELEM_BYTES);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(tbsv)(ctx->order, BMB_CBLAS_UPLO(ctx->uplo), BMB_CBLAS_TRANS(ctx->trans),
                    BMB_CBLAS_DIAG(ctx->diag), ctx->n, ctx->k, ctx->a, ctx->lda, ctx->x, 1);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->x, ctx->x0, (size_t) ctx->n * BMB_ELEM_BYTES);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->x0);
    free(ctx->x);
    free(ctx);
}

/* The band's elements inside the triangle: all N (N + 1) / 2 once K
 * reaches N - 1. */
static double bmb_band_elements(double n, double k)
{
    return k >= n - 1.0 ? n * (n + 1.0) / 2.0 : n * (k + 1.0) - k * (k + 1.0) / 2.0;
}

/* trsv's 2 a triangle element, over the band's elements. */
static double flops(size_t dim1, size_t dim2)
{
    return 2.0 * bmb_band_elements((double) dim1, (double) dim2);
}

/* Those elements read once, x read and written. */
static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    return (double) BMB_ELEM_BYTES * (bmb_band_elements(d1, (double) dim2) + 2.0 * d1);
}

/* --verify: the residual, in double. op(A), the band's triangle --uplo
 * gives with --trans and --diag, read straight from the band
 * (bmb_band_matvec), times the solution must give back the template,
 * which the driver has just restored x from. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, k = (size_t) ctx->k, i;
    double *ux = malloc(5 * n * sizeof(double));
    int ok;

    if (ux == NULL) {
        return -1;
    }
    double *uxabs = ux + n, *scale = uxabs + n, *x = scale + n, *x0 = x + n;

    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->x[0]);
    bmb_to_double(x, ctx->x, n);
    bmb_to_double(x0, ctx->x0, n);
    bmb_band_matvec(n, n, ctx->uplo == 'U' ? 0 : k, ctx->uplo == 'U' ? k : 0, ctx->trans, ctx->diag == 'U',
                    ctx->a, (size_t) ctx->lda, ctx->col, x, ux, uxabs);
    for (i = 0; i < n; i++) {
        scale[i] = uxabs[i] + fabs(x0[i]);
    }
    ok = bmb_verify_close(ctx->uplo == 'U' && ctx->trans == 'N' ? "U*x, the residual" : "op(A)*x, the residual",
                          ux, x0, scale, n, bmb_tolerance(n), msg, size);
    free(ux);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(tbsv);
    bench.dim1_label = "Matrix dim1 (N)";
    bench.dim2_label = "Matrix dim2 (K)";
    bench.use_vector_range = 0;
    bench.level = 2;
    bench.args = BMB_ARG_UPLO | BMB_ARG_TRANS | BMB_ARG_DIAG;
    bench.trans_letters = "NT";
    bench.setup = setup;
    bench.call = call;
    bench.reset = reset;
    bench.reset_every_call = 1;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
