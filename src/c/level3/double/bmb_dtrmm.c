#include <cblas.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* Side = Left: A is M x M triangular, B and C share the M x N shape.
 * dim1 = M, dim2 = N. B is overwritten in place by cblas_dtrmm(); it is
 * restored from b0 before every call so the benchmark is stable across
 * --iterations. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int ldb;
    int m;
    int n;
    double alpha;
    double *a;  /* M x M, upper triangle used */
    double *b0; /* template */
    double *b;  /* working buffer, restored by reset() before each call */
} bmb_ctx_t;

/* Off the diagonal, small enough to keep the matrix diagonally dominant
 * (at most 0.5 / n), and not symmetric: with a constant there, the upper
 * triangle read row-major equals the one read column-major, and --verify
 * could not tell a library that ignored --layout. */
static double bmb_off_diagonal(size_t i, size_t j, size_t n)
{
    return (double) ((i * 7 + j * 3) % 11 + 1) / (24.0 * (double) n);
}

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->alpha = 1.0;
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->ldb = ctx->col ? ctx->m : ctx->n;
    ctx->a = malloc(m * m * sizeof(double));
    ctx->b0 = malloc(m * n * sizeof(double));
    ctx->b = malloc(m * n * sizeof(double));
    if (ctx->a == NULL || ctx->b0 == NULL || ctx->b == NULL) {
        free(ctx->a);
        free(ctx->b0);
        free(ctx->b);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m; i++) {
        size_t j;

        for (j = 0; j < m; j++) {
            ctx->a[i * m + j] = (i == j) ? 2.0 : bmb_off_diagonal(i, j, m);
        }
    }
    for (i = 0; i < m * n; i++) {
        ctx->b0[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    memcpy(ctx->b, ctx->b0, m * n * sizeof(double));

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dtrmm(ctx->order, CblasLeft, CblasUpper, CblasNoTrans, CblasNonUnit,
                ctx->m, ctx->n, ctx->alpha, ctx->a, ctx->m, ctx->b, ctx->ldb);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->b, ctx->b0, (size_t) ctx->m * (size_t) ctx->n * sizeof(double));
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b0);
    free(ctx->b);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return d2 * d1 * (d1 + 1.0);
}

/* --verify, by random projection: B*r against alpha*U*(B0*r), B0 being the
 * template the driver has just restored B from. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, i;
    double *r = malloc((n + 8 * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + n, *tabs = t + m, *y1 = tabs + m, *y1abs = y1 + m;
    double *y2 = y1abs + m, *y2abs = y2 + m, *want = y2abs + m, *scale = want + m;

    bmb_verify_probe(r, n);
    call(ctx);
    bmb_verify_perturb(&ctx->b[0]);
    bmb_verify_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b, ctx->ldb, r, NULL, y1, y1abs);
    bmb_verify_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b0, ctx->ldb, r, NULL, t, tabs);
    bmb_verify_matvec(BMB_VERIFY_UPPER, ctx->col, m, m, ctx->a, m, t, tabs, y2, y2abs);
    for (i = 0; i < m; i++) {
        want[i] = ctx->alpha * y2[i];
        scale[i] = y1abs[i] + fabs(ctx->alpha) * y2abs[i];
    }
    ok = bmb_verify_close("B*r, for a random vector r", y1, want, scale, m,
                          bmb_verify_tolerance(m + n), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dtrmm";
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

    return bmb_benchmark_main(argc, argv, &bench);
}
