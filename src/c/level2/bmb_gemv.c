#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sgemv, dgemv, cgemv and zgemv, one per precision (bmb_prec.h): y =
 * alpha * op(A) * x, op as --trans gives it (#221).
 * dim1 = M (rows of A), dim2 = N (columns of A); x has N elements and y M,
 * or the other way round when A is transposed. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int m;
    int n;
    char trans; /* --trans: N, T or C */
    int nx;     /* x's length: N, or M transposed */
    int ny;     /* y's: M, or N */
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t beta[BMB_PARTS];
    bmb_real_t *a; /* M x N, in the order of --layout */
    bmb_real_t *x; /* nx */
    bmb_real_t *y; /* ny */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    const int col = bmb_bench_column_major();
    /* A as stored: its rows, or its columns with --layout col. */
    const size_t outer = col ? dim2 : dim1;
    const size_t lda = bmb_bench_ld(outer, col ? dim1 : dim2, BMB_ELEM_BYTES);
    const char trans = bmb_bench_trans(0);
    const size_t nx = trans == 'N' ? dim2 : dim1, ny = trans == 'N' ? dim1 : dim2;
    size_t i;

    (void) thread_count;

#if BMB_ELEM_BYTES > 8
    if (lda == 0) {
        bmb_bench_too_large();
        return NULL;
    }
#endif
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) dim1;
    ctx->n = (int) dim2;
    ctx->trans = trans;
    ctx->nx = (int) nx;
    ctx->ny = (int) ny;
    bmb_set_scalar(ctx->alpha, BMB_R(1.0), BMB_R(0.0));
    bmb_set_scalar(ctx->beta, BMB_R(0.0), BMB_R(0.0));
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->a = bmb_bench_alloc(outer * lda * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(nx * BMB_ELEM_BYTES);
    ctx->y = bmb_bench_alloc(ny * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->x == NULL || ctx->y == NULL) {
        free(ctx->a);
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, outer * lda, 0, 31);
    bmb_fill(ctx->x, nx, 0, 37);
    for (i = 0; i < BMB_PARTS * ny; i++) {
        ctx->y[i] = BMB_R(0.0);
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(gemv)(ctx->order, BMB_CBLAS_TRANS(ctx->trans), ctx->m, ctx->n, BMB_SCALAR(ctx->alpha),
                    (const void *) ctx->a, ctx->lda, (const void *) ctx->x, 1,
                    BMB_SCALAR(ctx->beta), (void *) ctx->y, 1);
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

    return BMB_MADD_FLOPS * d1 * d2;
}

/* A read once, x read once, y read and written: x and y swap lengths
 * when A is transposed. */
static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;
    const double nx = bmb_bench_trans(0) == 'N' ? d2 : d1, ny = bmb_bench_trans(0) == 'N' ? d1 : d2;

    return (double) BMB_ELEM_BYTES * (d1 * d2 + nx + 2.0 * ny);
}

/* --verify: alpha * op(A) * x + beta * y0, recomputed in double. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t m = (size_t) ctx->ny, n = (size_t) ctx->nx, i;
    double *y0 = malloc(((5 * p + 1) * m + p * n) * sizeof(double));
    const bmb_real_t *al = ctx->alpha, *be = ctx->beta;
    int ok;

    if (y0 == NULL) {
        return -1;
    }
    double *t = y0 + p * m, *tabs = t + p * m, *want = tabs + m, *scale = want + p * m;
    double *got = scale + p * m, *x = got + p * m;

    bmb_to_double(y0, ctx->y, m);
    bmb_to_double(x, ctx->x, n);
    call(ctx);
    bmb_perturb(&ctx->y[0]);
    bmb_matvec(BMB_VERIFY_OP(ctx->trans), ctx->col, (size_t) ctx->m, (size_t) ctx->n, ctx->a,
               (size_t) ctx->lda, x, NULL, t, tabs);
    bmb_to_double(got, ctx->y, m);
    for (i = 0; i < m; i++) {
#if BMB_COMPLEX
        const double tr = t[2 * i], ti = t[2 * i + 1], yr = y0[2 * i], yi = y0[2 * i + 1];

        want[2 * i] = al[0] * tr - al[1] * ti + (be[0] * yr - be[1] * yi);
        want[2 * i + 1] = al[0] * ti + al[1] * tr + (be[0] * yi + be[1] * yr);
        scale[2 * i] = scale[2 * i + 1] = bmb_abs(al) * tabs[i] + bmb_abs(be) * (fabs(yr) + fabs(yi));
#else
        want[i] = al[0] * t[i] + be[0] * y0[i];
        scale[i] = fabs(al[0]) * tabs[i] + fabs(be[0] * y0[i]);
#endif
    }
    ok = bmb_verify_close("y" BMB_PARTS_NOTE, got, want, scale, p * m, bmb_tolerance(n), msg, size);
    free(y0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(gemv);
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.level = 2;
    bench.args = BMB_ARG_TRANS;
    bench.trans_letters = BMB_TRANS_LETTERS;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
