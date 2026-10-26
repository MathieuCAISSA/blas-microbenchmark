#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* ssyr2 and dsyr2, one per precision (bmb_prec.h); the complex one,
 * her2, is a family of its own.
 * Square routine: only dim1 (= N) is used, swept via --matrix-dim1.
 * A (N x N, the triangle --uplo gives) is updated in place: A += alpha*(x*y^T + y*x^T). */
#if BMB_COMPLEX
#error "syr2 is real: the complex one is her2"
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
    bmb_real_t *y;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1;
    const size_t lda = bmb_bench_ld(n, n, BMB_ELEM_BYTES);

    (void) dim2;
    (void) thread_count;

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
    bmb_fill(ctx->y, n, 7, 0);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(syr2)(ctx->order, BMB_CBLAS_UPLO(ctx->uplo), ctx->n, ctx->alpha, ctx->x, 1, ctx->y, 1,
                    ctx->a, ctx->lda);
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
    return 2.0 * d1 * (d1 + 1.0);
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (double) BMB_ELEM_BYTES * (d1 * (d1 + 1.0) + 2.0 * d1);
}

/* --verify, by random projection in double: A*r against A0*r plus the
 * update's upper triangle times r, the strictly lower part having to
 * stay as it was -- which the projection of the whole matrix also
 * checks. */
static int verify_at(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->n, n = (size_t) ctx->n, i, k;
    double *r = malloc((n + 6 * m) * sizeof(double));
    const double alpha = ctx->alpha;
    double s1 = 0.0, s1abs = 0.0, s2 = 0.0, s2abs = 0.0;
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *p0 = r + n, *p0abs = p0 + m, *p = p0 + 2 * m, *pabs = p0 + 3 * m;
    double *want = p0 + 4 * m, *scale = p0 + 5 * m;

    bmb_verify_probe(r, n);
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->a, (size_t) ctx->lda, r, NULL, p0, p0abs);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->a[0]);
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->a, (size_t) ctx->lda, r, NULL, p, pabs);
    /* Row i of the update's triangle, times r: x_i (or y_i) times the
     * sums of x_j r_j (y_j r_j) over the columns the triangle has in that
     * row, j >= i in the upper one, j <= i in the lower, gathered row by
     * row from the end of the triangle the sums start at. */
    for (k = 0; k < m; k++) {
        i = ctx->uplo == 'U' ? m - 1 - k : k;
        const double xi = ctx->x[i], yi = ctx->y[i];

        s1 += yi * r[i];
        s1abs += fabs(yi * r[i]);
        s2 += xi * r[i];
        s2abs += fabs(xi * r[i]);
        want[i] = p0[i] + alpha * (xi * s1 + yi * s2);
        scale[i] = pabs[i] + p0abs[i] + fabs(alpha) * (fabs(xi) * s1abs + fabs(yi) * s2abs);
    }
    ok = bmb_verify_close("A*r, for a random vector r", p, want, scale, m, bmb_tolerance(n), msg, size);
    free(r);
    return !ok;
}

/* --verify makes its call with an alpha of order 1, then gives the timed
 * one back: the timed alpha is kept small so that repeated calls do not
 * drift, and its update would fall inside single precision's tolerance,
 * where a check could not tell it from no call at all (#239). */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const bmb_real_t timed = ctx->alpha;
    int rc;

    ctx->alpha = BMB_R(0.5);
    rc = verify_at(vctx, msg, size);
    ctx->alpha = timed;
    return rc;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(syr2);
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
