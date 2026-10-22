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
    const int col = bmb_bench_column_major();
    /* K is M: A is M x M, B and C both M x N, each stored as its rows, or
     * its columns with --layout col. */
    const size_t outa = m, outb = col ? n : m, outc = outb;
    const size_t lda = bmb_bench_ld(outa, m, sizeof(float));
    const size_t ldb = bmb_bench_ld(outb, col ? m : n, sizeof(float));
    const size_t ldc = ldb;
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
    ctx->beta = 0.0f; /* verify() relies on it */
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->ldb = (int) ldb;
    ctx->ldc = (int) ldc;
    ctx->a = bmb_bench_alloc(outa * lda * sizeof(float));
    ctx->b = bmb_bench_alloc(outb * ldb * sizeof(float));
    ctx->c = bmb_bench_alloc(outc * ldc * sizeof(float));
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < outa * lda; i++) {
        ctx->a[i] = (float) (i % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < outb * ldb; i++) {
        ctx->b[i] = (float) ((i + 7) % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < outc * ldc; i++) {
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

/* --cache cold: the bytes of A, B and C: M x K (K = M), K x N, M x N. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return (d1 * d1 + 2.0 * d1 * d2) * (double) (sizeof(float));
}

/* --verify, by random projection in double: C*r against alpha*A*(B*r).
 * beta is 0 (setup), so what C held before the call does not enter. The
 * library rounded each element of C through K float multiply-adds, an
 * error of at most the tolerance times |alpha||A||B| there, so at most
 * the tolerance times |alpha||A|(|B||r|) on C*r: the scale leaves out
 * |C||r|, which dgemm's has. The reference's own rounding, in double, is
 * 2^-29 of that. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, inner = (size_t) ctx->k, i;
    double *r = malloc((n + 2 * inner + 6 * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + n, *tabs = t + inner;
    double *y1 = tabs + inner, *y1abs = y1 + m, *y2 = y1abs + m, *y2abs = y2 + m;
    double *want = y2abs + m, *scale = want + m;

    bmb_verify_probe(r, n);
    call(ctx);
    bmb_verify_perturb_single(&ctx->c[0]);
    bmb_verify_matvec_single(BMB_VERIFY_FULL, ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, y1, y1abs);
    bmb_verify_matvec_single(BMB_VERIFY_FULL, ctx->col, inner, n, ctx->b, (size_t) ctx->ldb, r, NULL, t, tabs);
    bmb_verify_matvec_single(BMB_VERIFY_FULL, ctx->col, m, inner, ctx->a, (size_t) ctx->lda, t, tabs, y2, y2abs);
    for (i = 0; i < m; i++) {
        want[i] = ctx->alpha * y2[i];
        scale[i] = fabs(ctx->alpha) * y2abs[i];
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
    bench.level = 3;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
