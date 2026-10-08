#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* dim1 = M = K, dim2 = N, as for dgemm. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldb;
    int ldc;
    int m;
    int n;
    int k;
    float alpha;
    float beta;
    float *a; /* M x K */
    float *b; /* K x N */
    float *c; /* M x N */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2, k = dim1;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->alpha = 1.0f;
    ctx->beta = 0.0f;
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = ctx->col ? ctx->m : ctx->k;
    ctx->ldb = ctx->col ? ctx->k : ctx->n;
    ctx->ldc = ctx->col ? ctx->m : ctx->n;
    ctx->a = malloc(m * k * sizeof(float));
    ctx->b = malloc(k * n * sizeof(float));
    ctx->c = malloc(m * n * sizeof(float));
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m * k; i++) {
        ctx->a[i] = (float) (i % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < k * n; i++) {
        ctx->b[i] = (float) ((i + 7) % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < m * n; i++) {
        ctx->c[i] = 0.0f;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_sgemm(ctx->order, CblasNoTrans, CblasNoTrans, ctx->m, ctx->n, ctx->k,
                ctx->alpha, ctx->a, ctx->lda, ctx->b, ctx->ldb, ctx->beta, ctx->c, ctx->ldc);
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

/* --verify, by random projection in double: C*r against
 * alpha*A*(B*r) + beta*C0*r. The library rounded each element of C
 * through K float multiply-adds, an error of at most the tolerance times
 * |alpha||A||B| + |beta||C0| there, so at most the tolerance times
 * |alpha||A|(|B||r|) + |beta||C0||r| on C*r: the scale leaves out |C||r|,
 * which dgemm's has. The reference's own rounding, in double, is 2^-29
 * of that. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, inner = (size_t) ctx->k, i;
    double *r = malloc((n + 2 * inner + 8 * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + n, *tabs = t + inner, *c0 = tabs + inner, *c0abs = c0 + m;
    double *y1 = c0abs + m, *y1abs = y1 + m, *y2 = y1abs + m, *y2abs = y2 + m;
    double *want = y2abs + m, *scale = want + m;

    bmb_verify_probe(r, n);
    for (i = 0; i < m; i++) {
        c0[i] = c0abs[i] = 0.0;
    }
    if (ctx->beta != 0.0f) {
        bmb_verify_matvec_single(ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, c0, c0abs);
    }
    call(ctx);
    bmb_verify_perturb_single(&ctx->c[0]);
    bmb_verify_matvec_single(ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, y1, y1abs);
    bmb_verify_matvec_single(ctx->col, inner, n, ctx->b, (size_t) ctx->ldb, r, NULL, t, tabs);
    bmb_verify_matvec_single(ctx->col, m, inner, ctx->a, (size_t) ctx->lda, t, tabs, y2, y2abs);
    for (i = 0; i < m; i++) {
        want[i] = ctx->alpha * y2[i] + ctx->beta * c0[i];
        scale[i] = fabs(ctx->alpha) * y2abs[i] + fabs(ctx->beta) * c0abs[i];
    }
    ok = bmb_verify_close("C*r, for a random vector r", y1, want, scale, m,
                          bmb_verify_tolerance_single(inner), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "sgemm";
    bench.dim1_label = "Matrix dim1 (M=K)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;

    return bmb_benchmark_main(argc, argv, &bench);
}
