#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* Side = Left: A is M x M Hermitian, B and C are M x N, as for dsymm.
 * dim1 = M, dim2 = N. Every complex number is a pair of doubles, real
 * part first, as for zgemm. A's diagonal has an imaginary part, which a
 * Hermitian matrix cannot have and BLAS does not read: --verify ignores it
 * too, so a library that used it would be caught. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int ldb;
    int m;
    int n;
    double alpha[2];
    double beta[2];
    double *a; /* M x M, upper triangle used */
    double *b; /* M x N */
    double *c; /* M x N */
} bmb_ctx_t;

/* A double complex element is a pair of doubles: 16 bytes, past what the
 * parser's ceiling covers (bmb_bench_matrix_bytes). */
#define BMB_Z (2 * sizeof(double))

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2;
    size_t i;

    (void) thread_count;

    if (bmb_bench_matrix_bytes(m, m, BMB_Z) == 0 || bmb_bench_matrix_bytes(m, n, BMB_Z) == 0) {
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
    ctx->beta[0] = 0.0;
    ctx->beta[1] = 0.0;
    ctx->col = bmb_bench_column_major();
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->ldb = ctx->col ? ctx->m : ctx->n;
    ctx->a = malloc(bmb_bench_matrix_bytes(m, m, BMB_Z));
    ctx->b = malloc(bmb_bench_matrix_bytes(m, n, BMB_Z));
    ctx->c = malloc(bmb_bench_matrix_bytes(m, n, BMB_Z));
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < m * m; i++) {
        ctx->a[2 * i] = (double) (i % 100) * 0.01 - 0.5;
        ctx->a[2 * i + 1] = (double) ((i + 31) % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < m * n; i++) {
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

    cblas_zhemm(ctx->order, CblasLeft, CblasUpper, ctx->m, ctx->n,
                (const void *) ctx->alpha, (const void *) ctx->a, ctx->m,
                (const void *) ctx->b, ctx->ldb, (const void *) ctx->beta,
                (void *) ctx->c, ctx->ldb);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b);
    free(ctx->c);
    free(ctx);
}

/* dsymm's count, four times over: 8*M*M*N. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return 8.0 * d1 * d1 * d2;
}

/* --verify, by random projection: C*r against alpha*A*(B*r) + beta*C0*r,
 * A Hermitian, for a complex r. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, i;
    const double *al = ctx->alpha, *be = ctx->beta;
    double *r = malloc((2 * n + 16 * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + 2 * n, *tabs = t + 2 * m, *c0 = tabs + m, *c0abs = c0 + 2 * m;
    double *y1 = c0abs + m, *y1abs = y1 + 2 * m, *y2 = y1abs + m, *y2abs = y2 + 2 * m;
    double *want = y2abs + m, *scale = want + 2 * m;

    bmb_verify_probe(r, 2 * n);
    for (i = 0; i < m; i++) {
        c0[2 * i] = c0[2 * i + 1] = c0abs[i] = 0.0;
    }
    if (be[0] != 0.0 || be[1] != 0.0) {
        bmb_verify_matvec_complex(BMB_VERIFY_FULL, ctx->col, m, n, ctx->c, ctx->ldb, r, NULL, c0, c0abs);
    }
    call(ctx);
    bmb_verify_perturb(&ctx->c[0]);
    bmb_verify_matvec_complex(BMB_VERIFY_FULL, ctx->col, m, n, ctx->c, ctx->ldb, r, NULL, y1, y1abs);
    bmb_verify_matvec_complex(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b, ctx->ldb, r, NULL, t, tabs);
    bmb_verify_matvec_complex(BMB_VERIFY_HERM_UPPER, ctx->col, m, m, ctx->a, m, t, tabs, y2, y2abs);
    for (i = 0; i < m; i++) {
        double yr = y2[2 * i], yi = y2[2 * i + 1], cr = c0[2 * i], ci = c0[2 * i + 1];

        want[2 * i] = al[0] * yr - al[1] * yi + be[0] * cr - be[1] * ci;
        want[2 * i + 1] = al[0] * yi + al[1] * yr + be[0] * ci + be[1] * cr;
        scale[2 * i] = scale[2 * i + 1] = y1abs[i] + (fabs(al[0]) + fabs(al[1])) * y2abs[i]
                                          + (fabs(be[0]) + fabs(be[1])) * c0abs[i];
    }
    ok = bmb_verify_close("C*r, for a random vector r (real and imaginary parts in turn)",
                          y1, want, scale, 2 * m, bmb_verify_tolerance(2 * (m + n)), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "zhemm";
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
