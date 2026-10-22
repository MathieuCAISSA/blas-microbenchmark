#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* dim1 = N, dim2 = K, as for dsyrk. C (N x N, Hermitian) = alpha * A * A^H
 * + beta * C, A is N x K; alpha and beta are real, as zherk takes them.
 * Every complex number is a pair of doubles, real part first, as for
 * zgemm. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldc;
    int n;
    int k;
    double alpha;
    double beta;
    double *a; /* N x K */
    double *c; /* N x N, upper triangle used */
} bmb_ctx_t;

/* A double complex element is a pair of doubles: 16 bytes, past what the
 * parser's ceiling covers (bmb_bench_matrix_bytes). */
#define BMB_Z (2 * sizeof(double))

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1, k = dim2;
    const int col = bmb_bench_column_major();
    /* A as stored: its rows, or its columns with --layout col. */
    const size_t outa = col ? k : n;
    const size_t lda = bmb_bench_ld(outa, col ? n : k, BMB_Z);
    const size_t ldc = bmb_bench_ld(n, n, BMB_Z);
    size_t i;

    (void) thread_count;

    if (lda == 0 || ldc == 0) {
        bmb_bench_too_large();
        return NULL;
    }
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->alpha = 1.0;
    ctx->beta = 0.0; /* verify() relies on it */
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->ldc = (int) ldc;
    ctx->a = bmb_bench_alloc(outa * lda * BMB_Z);
    ctx->c = bmb_bench_alloc(n * ldc * BMB_Z);
    if (ctx->a == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < outa * lda; i++) {
        ctx->a[2 * i] = (double) (i % 100) * 0.01 - 0.5;
        ctx->a[2 * i + 1] = (double) ((i + 31) % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < 2 * n * ldc; i++) {
        ctx->c[i] = 0.0;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_zherk(ctx->order, CblasUpper, CblasNoTrans, ctx->n, ctx->k,
                ctx->alpha, (const void *) ctx->a, ctx->lda, ctx->beta, (void *) ctx->c, ctx->ldc);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->c);
    free(ctx);
}

/* dsyrk's count, four times over: a complex multiply-add is 8 real
 * operations, where dsyrk counts 2. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return 4.0 * d2 * d1 * (d1 + 1.0);
}

/* --cache cold: the bytes of A and C: N x K, N x N. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return (d1 * d2 + d1 * d1) * (double) (BMB_Z);
}

/* --verify, by random projection: Herm(C)*r against alpha*A*(A^H*r)
 * for a complex r, Herm(C) being the Hermitian matrix C's upper triangle
 * stands for; beta is 0 (setup), so what C held before the call does not
 * enter. The strictly lower
 * part, which the routine must not touch, is projected before and after
 * the call, and must come out the same to the bit, as for dsyrk. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, k = (size_t) ctx->k, i;
    double *r = malloc((17 * n + 3 * k) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + 2 * n, *tabs = t + 2 * k;
    double *y1 = tabs + k, *y1abs = y1 + 2 * n, *y2 = y1abs + n, *y2abs = y2 + 2 * n;
    double *l0 = y2abs + n, *l = l0 + 2 * n, *labs = l + 2 * n;
    double *want = labs + n, *scale = want + 2 * n;

    bmb_verify_probe(r, 2 * n);
    bmb_verify_matvec_complex(BMB_VERIFY_LOWER_STRICT, ctx->col, n, n, ctx->c, ctx->ldc, r, NULL, l0, labs);
    call(ctx);
    bmb_verify_perturb(&ctx->c[0]);
    bmb_verify_matvec_complex(BMB_VERIFY_HERM_UPPER, ctx->col, n, n, ctx->c, ctx->ldc, r, NULL, y1, y1abs);
    bmb_verify_matvec_complex(BMB_VERIFY_FULL_H, ctx->col, n, k, ctx->a, ctx->lda, r, NULL, t, tabs);
    bmb_verify_matvec_complex(BMB_VERIFY_FULL, ctx->col, n, k, ctx->a, ctx->lda, t, tabs, y2, y2abs);
    for (i = 0; i < 2 * n; i++) {
        want[i] = ctx->alpha * y2[i];
        scale[i] = y1abs[i / 2] + fabs(ctx->alpha) * y2abs[i / 2];
    }
    ok = bmb_verify_close("C*r, for a random vector r (real and imaginary parts in turn)", y1, want,
                          scale, 2 * n, bmb_verify_tolerance(2 * (k + n)), msg, size);
    if (ok) {
        bmb_verify_matvec_complex(BMB_VERIFY_LOWER_STRICT, ctx->col, n, n, ctx->c, ctx->ldc, r, NULL, l, labs);
        ok = bmb_verify_close("the strictly lower part of C, times r, which the call must not change",
                              l, l0, NULL, 2 * n, 0.0, msg, size);
    }
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "zherk";
    bench.dim1_label = "Matrix dim1 (N)";
    bench.dim2_label = "Matrix dim2 (K)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
