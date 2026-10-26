#include <cblas.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"
#include "bmb_packed.h"

/* stpsv and dtpsv, one per precision (bmb_prec.h): trsv on a triangular
 * A packed (bmb_packed.h), the triangle --uplo gives kept in N (N + 1) / 2
 * elements. Square routine: only dim1 (= N) is used. X is overwritten in
 * place by the call; it is restored from x0 before every call so the
 * benchmark is stable across --iterations. The complex ones, ctpsv and
 * ztpsv, are not here yet. */
#if BMB_COMPLEX
#error "tpsv is real here: ctpsv and ztpsv are not here yet"
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int n;
    char uplo, trans, diag; /* --uplo, --trans, --diag */
    bmb_real_t *ap; /* N (N + 1) / 2, packed in the order of --layout */
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
    size_t n = dim1;
    size_t i;

    (void) dim2;
    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->uplo = bmb_bench_uplo();
    ctx->trans = bmb_bench_trans(0);
    ctx->diag = bmb_bench_diag();
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->ap = bmb_bench_alloc(n * (n + 1) / 2 * BMB_ELEM_BYTES);
    ctx->x0 = bmb_bench_alloc(n * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(n * BMB_ELEM_BYTES);
    if (ctx->ap == NULL || ctx->x0 == NULL || ctx->x == NULL) {
        free(ctx->ap);
        free(ctx->x0);
        free(ctx->x);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < n; i++) {
        const size_t lo = ctx->uplo == 'U' ? i : 0, hi = ctx->uplo == 'U' ? n : i + 1;
        size_t j;

        for (j = lo; j < hi; j++) {
            /* Diagonally dominant so the triangular solve is well-conditioned. */
            ctx->ap[bmb_packed_index(i, j, n, ctx->col, ctx->uplo)] =
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

    BMB_CBLAS(tpsv)(ctx->order, BMB_CBLAS_UPLO(ctx->uplo), BMB_CBLAS_TRANS(ctx->trans),
                    BMB_CBLAS_DIAG(ctx->diag), ctx->n, ctx->ap, ctx->x, 1);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->x, ctx->x0, (size_t) ctx->n * BMB_ELEM_BYTES);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->ap);
    free(ctx->x0);
    free(ctx->x);
    free(ctx);
}

/* trsv's: the same products, from the same N (N + 1) / 2 elements. */
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
    return (double) BMB_ELEM_BYTES * (d1 * (d1 + 1.0) / 2.0 + 2.0 * d1);
}

/* --verify: the residual, in double. op(A), the triangle --uplo gives
 * with --trans and --diag, read straight from its packed elements
 * (bmb_packed_matvec), times the solution must give back the template,
 * which the driver has just restored x from. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, i;
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
    bmb_packed_matvec(0, ctx->uplo, ctx->trans, ctx->diag, ctx->ap, n, ctx->col, x, ux, uxabs);
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

    bench.routine_name = BMB_ROUTINE(tpsv);
    bench.dim1_label = "Matrix dim (N)";
    bench.dim2_label = NULL;
    bench.use_vector_range = 0;
    bench.packed = 1;
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
