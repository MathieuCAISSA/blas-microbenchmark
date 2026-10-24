#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sger and dger, one per precision (bmb_prec.h); the complex rank-1
 * updates, geru and gerc, are families of their own.
 * dim1 = M, dim2 = N. A (M x N) is updated in place: A += alpha * x * y^T. */
#if BMB_COMPLEX
#error "ger is real: the complex updates are geru and gerc"
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int m;
    int n;
    bmb_real_t alpha;
    bmb_real_t *a;
    bmb_real_t *x;
    bmb_real_t *y;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    const int col = bmb_bench_column_major();
    /* A as stored: its rows, or its columns with --layout col. */
    const size_t outer = col ? dim2 : dim1;
    const size_t lda = bmb_bench_ld(outer, col ? dim1 : dim2, BMB_ELEM_BYTES);

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) dim1;
    ctx->n = (int) dim2;
    /* Small alpha: A accumulates rank-1 updates across iterations, kept bounded
     * regardless of --iterations. */
    ctx->alpha = BMB_R(1.0e-6);
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->a = bmb_bench_alloc(outer * lda * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    ctx->y = bmb_bench_alloc(dim2 * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->x == NULL || ctx->y == NULL) {
        free(ctx->a);
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, outer * lda, 0, 0);
    bmb_fill(ctx->x, dim1, 0, 0);
    bmb_fill(ctx->y, dim2, 7, 0);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(ger)(ctx->order, ctx->m, ctx->n, ctx->alpha, ctx->x, 1, ctx->y, 1, ctx->a, ctx->lda);
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
    const double d2 = (double) dim2;

    return 2.0 * d1 * d2;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    return (double) BMB_ELEM_BYTES * (2.0 * d1 * d2 + d1 + d2);
}

/* --verify, by random projection in double: A*r against
 * A0*r + alpha*x*(y.r). */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, i, j;
    double *r = malloc((n + 6 * m) * sizeof(double));
    const double alpha = ctx->alpha;
    double s1 = 0.0, s1abs = 0.0;
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *p0 = r + n, *p0abs = p0 + m, *p = p0 + 2 * m, *pabs = p0 + 3 * m;
    double *want = p0 + 4 * m, *scale = p0 + 5 * m;

    bmb_verify_probe(r, n);
    for (j = 0; j < n; j++) {
        s1 += (double) ctx->y[j] * r[j];
        s1abs += fabs((double) ctx->y[j] * r[j]);
    }
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->a, (size_t) ctx->lda, r, NULL, p0, p0abs);
    call(ctx);
    bmb_perturb(&ctx->a[0]);
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->a, (size_t) ctx->lda, r, NULL, p, pabs);
    for (i = 0; i < m; i++) {
        want[i] = p0[i] + alpha * ctx->x[i] * s1;
        scale[i] = pabs[i] + p0abs[i] + fabs(alpha * ctx->x[i]) * s1abs;
    }
    ok = bmb_verify_close("A*r, for a random vector r", p, want, scale, m, bmb_tolerance(n), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(ger);
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.level = 2;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
