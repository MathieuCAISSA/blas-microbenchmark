#include <cblas.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* Side = Left: A is M x M upper triangular, B is M x N, as for dtrsm.
 * dim1 = M, dim2 = N. Every complex number is a pair of doubles, real
 * part first, as for zgemm. B is overwritten in place by cblas_ztrsm(); it
 * is restored from b0 before every call. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldb;
    int m;
    int n;
    double alpha[2];
    double *a;  /* M x M, upper triangle used, diagonally dominant */
    double *b0; /* template */
    double *b;  /* working buffer, restored by reset() before each call */
} bmb_ctx_t;

/* A double complex element is a pair of doubles: 16 bytes, past what the
 * parser's ceiling covers (bmb_bench_matrix_bytes). */
#define BMB_Z (2 * sizeof(double))

/* Off the diagonal, small enough to keep the matrix diagonally dominant,
 * and neither symmetric nor Hermitian, so that --verify can tell a
 * library that ignored --layout, or conjugated A. */
static double bmb_off_diagonal(size_t i, size_t j, size_t n)
{
    return (double) ((i * 7 + j * 3) % 11 + 1) / (48.0 * (double) n);
}

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2;
    const int col = bmb_bench_column_major();
    /* B as stored: its rows, or its columns with --layout col. */
    const size_t outb = col ? n : m;
    const size_t lda = bmb_bench_ld(m, m, BMB_Z);
    const size_t ldb = bmb_bench_ld(outb, col ? m : n, BMB_Z);
    size_t i;

    (void) thread_count;

    if (lda == 0 || ldb == 0) {
        bmb_bench_too_large();
        return NULL;
    }
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->alpha[0] = 1.0;
    ctx->alpha[1] = 0.0;
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->ldb = (int) ldb;
    ctx->a = bmb_bench_alloc(m * lda * BMB_Z);
    ctx->b0 = bmb_bench_alloc(outb * ldb * BMB_Z);
    ctx->b = bmb_bench_alloc(outb * ldb * BMB_Z);
    if (ctx->a == NULL || ctx->b0 == NULL || ctx->b == NULL) {
        free(ctx->a);
        free(ctx->b0);
        free(ctx->b);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m; i++) {
        size_t j;

        /* Through the padding too, so that no element of A is left
         * uninitialised. */
        for (j = 0; j < lda; j++) {
            double *e = &ctx->a[2 * (i * lda + j)];

            e[0] = (i == j) ? (double) (m + 1) : bmb_off_diagonal(i, j, m);
            e[1] = (i == j) ? 0.5 : bmb_off_diagonal(j + 5, i, m);
        }
    }
    for (i = 0; i < outb * ldb; i++) {
        ctx->b0[2 * i] = (double) (i % 100) * 0.01 + 1.0;
        ctx->b0[2 * i + 1] = (double) ((i + 37) % 100) * 0.01 - 0.5;
    }
    memcpy(ctx->b, ctx->b0, outb * ldb * BMB_Z);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_ztrsm(ctx->order, CblasLeft, CblasUpper, CblasNoTrans, CblasNonUnit,
                ctx->m, ctx->n, (const void *) ctx->alpha, (const void *) ctx->a, ctx->lda,
                (void *) ctx->b, ctx->ldb);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->b, ctx->b0, (size_t) (ctx->col ? ctx->n : ctx->m) * (size_t) ctx->ldb * BMB_Z);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b0);
    free(ctx->b);
    free(ctx);
}

/* dtrsm's count, four times over: a complex multiply-add is 8 real
 * operations, where dtrsm counts 2. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return 4.0 * d2 * d1 * (d1 + 1.0);
}

/* --cache cold: the bytes of A and B: M x M, M x N; the copy reset() restores B from is not touched by the call. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return (d1 * d1 + d1 * d2) * (double) (BMB_Z);
}

/* --verify, by random projection of the residual: U*(X*r) against
 * alpha*B0*r for a complex r, as dtrsm's. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, i;
    const double *al = ctx->alpha;
    double *r = malloc((2 * n + 13 * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + 2 * n, *tabs = t + 2 * m, *y1 = tabs + m, *y1abs = y1 + 2 * m;
    double *u = y1abs + m, *uabs = u + 2 * m, *want = uabs + m, *scale = want + 2 * m;

    bmb_verify_probe(r, 2 * n);
    call(ctx);
    bmb_verify_perturb(&ctx->b[0]);
    bmb_verify_matvec_complex(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b, ctx->ldb, r, NULL, t, tabs);
    bmb_verify_matvec_complex(BMB_VERIFY_UPPER, ctx->col, m, m, ctx->a, ctx->lda, t, tabs, y1, y1abs);
    bmb_verify_matvec_complex(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b0, ctx->ldb, r, NULL, u, uabs);
    for (i = 0; i < m; i++) {
        double ur = u[2 * i], ui = u[2 * i + 1];

        want[2 * i] = al[0] * ur - al[1] * ui;
        want[2 * i + 1] = al[0] * ui + al[1] * ur;
        scale[2 * i] = scale[2 * i + 1] = y1abs[i] + (fabs(al[0]) + fabs(al[1])) * uabs[i];
    }
    ok = bmb_verify_close("U*(X*r), the residual for a random vector r (real and imaginary parts in turn)",
                          y1, want, scale, 2 * m, bmb_verify_tolerance(2 * (m + n)), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "ztrsm";
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.reset = reset;
    bench.reset_every_call = 1;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
