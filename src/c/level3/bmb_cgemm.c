#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* dim1 = M = K, dim2 = N, as for dgemm. Every complex number is a pair of
 * floats, real part first, as for zgemm. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldb;
    int ldc;
    int m;
    int n;
    int k;
    float alpha[2];
    float beta[2];
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
    ctx->alpha[0] = 1.0f;
    ctx->alpha[1] = 0.0f;
    ctx->beta[0] = 0.0f;
    ctx->beta[1] = 0.0f;
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = ctx->col ? ctx->m : ctx->k;
    ctx->ldb = ctx->col ? ctx->k : ctx->n;
    ctx->ldc = ctx->col ? ctx->m : ctx->n;
    ctx->a = malloc(2 * m * k * sizeof(float));
    ctx->b = malloc(2 * k * n * sizeof(float));
    ctx->c = malloc(2 * m * n * sizeof(float));
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m * k; i++) {
        ctx->a[2 * i] = (float) (i % 100) * 0.01f - 0.5f;
        ctx->a[2 * i + 1] = (float) ((i + 31) % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < k * n; i++) {
        ctx->b[2 * i] = (float) ((i + 7) % 100) * 0.01f - 0.5f;
        ctx->b[2 * i + 1] = (float) ((i + 53) % 100) * 0.01f - 0.5f;
    }
    for (i = 0; i < 2 * m * n; i++) {
        ctx->c[i] = 0.0f;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_cgemm(ctx->order, CblasNoTrans, CblasNoTrans, ctx->m, ctx->n, ctx->k,
                (const void *) ctx->alpha, (const void *) ctx->a, ctx->lda,
                (const void *) ctx->b, ctx->ldb, (const void *) ctx->beta,
                (void *) ctx->c, ctx->ldc);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b);
    free(ctx->c);
    free(ctx);
}

/* A complex multiply-add is 4 real multiplications and 4 additions. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return 8.0 * d1 * d1 * d2;
}

/* --verify, by random projection in double: C*r against
 * alpha*A*(B*r) + beta*C0*r, for a complex r. The scale leaves out
 * |C||r|, as sgemm's does, for the same reason. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, inner = (size_t) ctx->k, i;
    const float *al = ctx->alpha, *be = ctx->beta;
    double *r = malloc((2 * n + 3 * inner + 13 * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + 2 * n, *tabs = t + 2 * inner, *c0 = tabs + inner, *c0abs = c0 + 2 * m;
    double *y1 = c0abs + m, *y1abs = y1 + 2 * m, *y2 = y1abs + m, *y2abs = y2 + 2 * m;
    double *want = y2abs + m, *scale = want + 2 * m;

    bmb_verify_probe(r, 2 * n);
    for (i = 0; i < m; i++) {
        c0[2 * i] = c0[2 * i + 1] = c0abs[i] = 0.0;
    }
    if (be[0] != 0.0f || be[1] != 0.0f) {
        bmb_verify_matvec_complex_single(ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, c0, c0abs);
    }
    call(ctx);
    bmb_verify_perturb_single(&ctx->c[0]);
    bmb_verify_matvec_complex_single(ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, y1, y1abs);
    bmb_verify_matvec_complex_single(ctx->col, inner, n, ctx->b, (size_t) ctx->ldb, r, NULL, t, tabs);
    bmb_verify_matvec_complex_single(ctx->col, m, inner, ctx->a, (size_t) ctx->lda, t, tabs, y2, y2abs);
    for (i = 0; i < m; i++) {
        double yr = y2[2 * i], yi = y2[2 * i + 1], cr = c0[2 * i], ci = c0[2 * i + 1];

        want[2 * i] = al[0] * yr - al[1] * yi + be[0] * cr - be[1] * ci;
        want[2 * i + 1] = al[0] * yi + al[1] * yr + be[0] * ci + be[1] * cr;
        scale[2 * i] = scale[2 * i + 1] = (fabs(al[0]) + fabs(al[1])) * y2abs[i]
                                          + (fabs(be[0]) + fabs(be[1])) * c0abs[i];
    }
    ok = bmb_verify_close("C*r, for a random vector r (real and imaginary parts in turn)",
                          y1, want, scale, 2 * m, bmb_verify_tolerance_single(2 * inner), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "cgemm";
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
