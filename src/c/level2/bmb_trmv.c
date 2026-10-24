#include <cblas.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* strmv, dtrmv, ctrmv and ztrmv, one per precision (bmb_prec.h).
 * Square routine: only dim1 (= N) is used, swept via --matrix-dim1.
 * X is overwritten in place by the call; it is restored from x0 before
 * every call so the benchmark is stable across --iterations. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int n;
    int lda;
    bmb_real_t *a;  /* N x N, in the order of --layout, upper triangle used */
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
    return (bmb_real_t) ((i * 7 + j * 3) % 11 + 1) / (BMB_R(24.0) * BMB_PARTS * (bmb_real_t) n);
}

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

    for (i = 0; i < n; i++) {
        size_t j;

        for (j = 0; j < n; j++) {
            bmb_real_t *e = &ctx->a[BMB_PARTS * (i * lda + j)];

            /* Diagonally dominant so repeated multiplication stays well
             * scaled even though X is reset before every call anyway. */
            e[0] = (i == j) ? BMB_R(2.0) : bmb_off_diagonal(i, j, n);
#if BMB_COMPLEX
            e[1] = (i == j) ? BMB_R(0.5) : bmb_off_diagonal(j + 5, i, n);
#endif
        }
        for (j *= BMB_PARTS; j < BMB_PARTS * lda; j++) {
            ctx->a[BMB_PARTS * i * lda + j] = BMB_R(0.0);
        }
    }
    bmb_fill(ctx->x0, n, 0, 37);
    memcpy(ctx->x, ctx->x0, n * BMB_ELEM_BYTES);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(trmv)(ctx->order, CblasUpper, CblasNoTrans, CblasNonUnit, ctx->n,
                    (const void *) ctx->a, ctx->lda, (void *) ctx->x, 1);
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

/* --verify: U times the template, recomputed in double. The driver has
 * just restored x from it. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t n = (size_t) ctx->n;
    double *want = malloc(4 * p * n * sizeof(double));
    int ok;

    if (want == NULL) {
        return -1;
    }
    double *scale = want + p * n, *x0 = scale + p * n, *got = x0 + p * n;

    bmb_to_double(x0, ctx->x0, n);
    call(ctx);
    bmb_perturb(&ctx->x[0]);
    bmb_to_double(got, ctx->x, n);
    bmb_matvec(BMB_VERIFY_UPPER, ctx->col, n, n, ctx->a, (size_t) ctx->lda, x0, NULL, want, scale);
#if BMB_COMPLEX
    {
        size_t i;

        /* One scale per element; the comparison takes one per part. */
        for (i = n; i-- > 0;) {
            scale[2 * i] = scale[2 * i + 1] = scale[i];
        }
    }
#endif
    ok = bmb_verify_close("x" BMB_PARTS_NOTE, got, want, scale, p * n, bmb_tolerance(n), msg, size);
    free(want);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(trmv);
    bench.dim1_label = "Matrix dim (N)";
    bench.dim2_label = NULL;
    bench.use_vector_range = 0;
    bench.level = 2;
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
