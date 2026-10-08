#include <cblas.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_log.h"
#include "bmb_verify.h"

/* dim1 = M = K, dim2 = N, as for dgemm. Every complex number is a pair of
 * doubles, real part first, the layout of C99's double complex and of
 * every CBLAS. They are passed as void *, which converts to whatever
 * pointer type a library's cblas.h declares for them. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldb;
    int ldc;
    int m;
    int n;
    int k;
    double alpha[2];
    double beta[2];
    double *a; /* M x K */
    double *b; /* K x N */
    double *c; /* M x N */
} bmb_ctx_t;

/* The bytes of a rows x cols complex matrix, or 0 where they would wrap
 * size_t: the ceiling the parser puts on a size is for a matrix of real
 * doubles, and a complex one takes twice the bytes. At -m 1073741824,
 * an M x K one would take 2^64, which wraps to 0, a malloc that
 * succeeds and a setup loop that overruns it. */
static size_t bmb_complex_bytes(size_t rows, size_t cols)
{
    if (rows > SIZE_MAX / (2 * sizeof(double)) / cols) {
        return 0;
    }
    return 2 * rows * cols * sizeof(double);
}

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2, k = dim1;
    size_t i;

    (void) thread_count;

    if (bmb_complex_bytes(m, k) == 0 || bmb_complex_bytes(k, n) == 0 || bmb_complex_bytes(m, n) == 0) {
        bmb_log_error("These complex matrices would take more bytes than size_t can count.");
        return NULL;
    }
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->alpha[0] = 1.0;
    ctx->alpha[1] = 0.0;
    ctx->beta[0] = 0.0;
    ctx->beta[1] = 0.0;
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = ctx->col ? ctx->m : ctx->k;
    ctx->ldb = ctx->col ? ctx->k : ctx->n;
    ctx->ldc = ctx->col ? ctx->m : ctx->n;
    ctx->a = malloc(bmb_complex_bytes(m, k));
    ctx->b = malloc(bmb_complex_bytes(k, n));
    ctx->c = malloc(bmb_complex_bytes(m, n));
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m * k; i++) {
        ctx->a[2 * i] = (double) (i % 100) * 0.01 - 0.5;
        ctx->a[2 * i + 1] = (double) ((i + 31) % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < k * n; i++) {
        ctx->b[2 * i] = (double) ((i + 7) % 100) * 0.01 - 0.5;
        ctx->b[2 * i + 1] = (double) ((i + 53) % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < 2 * m * n; i++) {
        ctx->c[i] = 0.0;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_zgemm(ctx->order, CblasNoTrans, CblasNoTrans, ctx->m, ctx->n, ctx->k,
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

/* --verify, by random projection: C*r against alpha*A*(B*r) + beta*C0*r,
 * for a complex r. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, inner = (size_t) ctx->k, i;
    const double *al = ctx->alpha, *be = ctx->beta;
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
    if (be[0] != 0.0 || be[1] != 0.0) {
        bmb_verify_matvec_complex(ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, c0, c0abs);
    }
    call(ctx);
    bmb_verify_perturb(&ctx->c[0]);
    bmb_verify_matvec_complex(ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, y1, y1abs);
    bmb_verify_matvec_complex(ctx->col, inner, n, ctx->b, (size_t) ctx->ldb, r, NULL, t, tabs);
    bmb_verify_matvec_complex(ctx->col, m, inner, ctx->a, (size_t) ctx->lda, t, tabs, y2, y2abs);
    for (i = 0; i < m; i++) {
        double yr = y2[2 * i], yi = y2[2 * i + 1], cr = c0[2 * i], ci = c0[2 * i + 1];

        want[2 * i] = al[0] * yr - al[1] * yi + be[0] * cr - be[1] * ci;
        want[2 * i + 1] = al[0] * yi + al[1] * yr + be[0] * ci + be[1] * cr;
        scale[2 * i] = scale[2 * i + 1] = y1abs[i] + (fabs(al[0]) + fabs(al[1])) * y2abs[i]
                                          + (fabs(be[0]) + fabs(be[1])) * c0abs[i];
    }
    ok = bmb_verify_close("C*r, for a random vector r (real and imaginary parts in turn)",
                          y1, want, scale, 2 * m, bmb_verify_tolerance(2 * (inner + n)), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "zgemm";
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
