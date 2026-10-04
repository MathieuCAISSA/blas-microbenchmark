#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* Side = Left: A is M x M symmetric, B and C are M x N.
 * dim1 = M, dim2 = N. */
typedef struct {
    int m;
    int n;
    double alpha;
    double beta;
    double *a; /* M x M, upper triangle used */
    double *b; /* M x N */
    double *c; /* M x N */
} bmb_ctx_t;

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
    ctx->beta = 0.0;
    ctx->a = malloc(m * m * sizeof(double));
    ctx->b = malloc(m * n * sizeof(double));
    ctx->c = malloc(m * n * sizeof(double));
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m * m; i++) {
        ctx->a[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < m * n; i++) {
        ctx->b[i] = (double) ((i + 7) % 100) * 0.01 - 0.5;
        ctx->c[i] = 0.0;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dsymm(CblasRowMajor, CblasLeft, CblasUpper, ctx->m, ctx->n,
                ctx->alpha, ctx->a, ctx->m, ctx->b, ctx->n, ctx->beta, ctx->c, ctx->n);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b);
    free(ctx->c);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return 2.0 * d1 * d1 * d2;
}

/* --verify, by random projection: C*r against alpha*A*(B*r) + beta*C0*r, A symmetric. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, inner = (size_t) ctx->m, i;
    double *r = malloc((n + 2 * inner + 8 * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + n, *tabs = t + inner, *c0 = tabs + inner, *c0abs = c0 + m;
    double *y1 = c0abs + m, *y1abs = y1 + m, *y2 = y1abs + m, *y2abs = y2 + m;
    double *want = y2abs + m, *scale = want + m;

    (void) inner;
    bmb_verify_probe(r, n);
    for (i = 0; i < m; i++) {
        c0[i] = c0abs[i] = 0.0;
    }
    if (ctx->beta != 0.0) {
        bmb_verify_matvec(BMB_VERIFY_FULL, m, n, ctx->c, n, r, NULL, c0, c0abs);
    }
    call(ctx);
    bmb_verify_perturb(&ctx->c[0]);
    bmb_verify_matvec(BMB_VERIFY_FULL, m, n, ctx->c, n, r, NULL, y1, y1abs);
    bmb_verify_matvec(BMB_VERIFY_FULL, m, n, ctx->b, n, r, NULL, t, tabs);
    bmb_verify_matvec(BMB_VERIFY_SYM_UPPER, m, m, ctx->a, m, t, tabs, y2, y2abs);
    for (i = 0; i < m; i++) {
        want[i] = ctx->alpha * y2[i] + ctx->beta * c0[i];
        scale[i] = y1abs[i] + fabs(ctx->alpha) * y2abs[i] + fabs(ctx->beta) * c0abs[i];
    }
    ok = bmb_verify_close("C*r, for a random vector r", y1, want, scale, m,
                          bmb_verify_tolerance(inner + n), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dsymm";
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;

    return bmb_benchmark_main(argc, argv, &bench);
}
