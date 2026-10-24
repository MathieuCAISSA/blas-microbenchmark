#include <cblas.h>
#include <stdio.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sgemm, dgemm, cgemm and zgemm, one per precision (bmb_prec.h).
 * dim1 = M, dim2 = N, and K, which -K gives apart (#215) and is M
 * otherwise, as it always was. C = alpha * op(A) * op(B), op as --trans
 * gives it, A's then B's (#221): op(A) is M x K, so a transposed A is
 * stored K x M; the same for B. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldb;
    int ldc;
    int m;
    int n;
    int k;
    char ta; /* --trans: N, T or C, A's */
    char tb; /* B's */
    int ra, ca, rb, cb; /* A's and B's rows and columns, as stored */
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t beta[BMB_PARTS];
    bmb_real_t *a; /* M x K, or K x M transposed */
    bmb_real_t *b; /* K x N, or N x K transposed */
    bmb_real_t *c; /* M x N */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2, k = bmb_bench_dim3(dim1);
    const int col = bmb_bench_column_major();
    const char ta = bmb_bench_trans(0), tb = bmb_bench_trans(1);
    /* A is ra x ca, B rb x cb, C M x N, each stored as its rows, or its
     * columns with --layout col. */
    const size_t ra = ta == 'N' ? m : k, ca = ta == 'N' ? k : m;
    const size_t rb = tb == 'N' ? k : n, cb = tb == 'N' ? n : k;
    const size_t outa = col ? ca : ra, outb = col ? cb : rb, outc = col ? n : m;
    const size_t lda = bmb_bench_ld(outa, col ? ra : ca, BMB_ELEM_BYTES);
    const size_t ldb = bmb_bench_ld(outb, col ? rb : cb, BMB_ELEM_BYTES);
    const size_t ldc = bmb_bench_ld(outc, col ? m : n, BMB_ELEM_BYTES);
    size_t i;

    (void) thread_count;

#if BMB_ELEM_BYTES > 8
    if (lda == 0 || ldb == 0 || ldc == 0) {
        bmb_bench_too_large();
        return NULL;
    }
#endif
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->ta = ta;
    ctx->tb = tb;
    ctx->ra = (int) ra;
    ctx->ca = (int) ca;
    ctx->rb = (int) rb;
    ctx->cb = (int) cb;
    bmb_set_scalar(ctx->alpha, BMB_R(1.0), BMB_R(0.0));
    bmb_set_scalar(ctx->beta, BMB_R(0.0), BMB_R(0.0)); /* verify() relies on it */
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->ldb = (int) ldb;
    ctx->ldc = (int) ldc;
    ctx->a = bmb_bench_alloc(outa * lda * BMB_ELEM_BYTES);
    ctx->b = bmb_bench_alloc(outb * ldb * BMB_ELEM_BYTES);
    ctx->c = bmb_bench_alloc(outc * ldc * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, outa * lda, 0, 31);
    bmb_fill(ctx->b, outb * ldb, 7, 53);
    for (i = 0; i < BMB_PARTS * outc * ldc; i++) {
        ctx->c[i] = BMB_R(0.0);
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(gemm)(ctx->order, BMB_CBLAS_TRANS(ctx->ta), BMB_CBLAS_TRANS(ctx->tb), ctx->m, ctx->n, ctx->k,
                    BMB_SCALAR(ctx->alpha), (const void *) ctx->a, ctx->lda,
                    (const void *) ctx->b, ctx->ldb, BMB_SCALAR(ctx->beta),
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

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;
    const double k = (double) bmb_bench_dim3(dim1);

    return BMB_MADD_FLOPS * d1 * k * d2;
}

/* --cache cold: the bytes of A, B and C: M x K, K x N, M x N. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;
    const double k = (double) bmb_bench_dim3(dim1);

    return (d1 * k + k * d2 + d1 * d2) * (double) (BMB_ELEM_BYTES);
}

/* --verify, by random projection in double: C*r against
 * alpha*op(A)*(op(B)*r), op as --trans gives it, r
 * of the routine's kind, real or complex. beta is 0 (setup), so what C
 * held before the call does not enter. In single precision, the library
 * rounded each element of C through K float multiply-adds, an error of
 * at most the tolerance times |alpha||A||B| there, so at most the
 * tolerance times |alpha||A|(|B||r|) on C*r: the scale leaves out
 * |C||r|, which double precision's has, and the tolerance counts the
 * library's terms alone. The reference's own rounding, in double, is
 * 2^-29 of that. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, inner = (size_t) ctx->k, i;
    double *r;
    int ok;

    /* The call's K is the point's (-K, #215): a setup that kept K = M
     * would pass the projection, which reads the K the call was given. */
    if (inner != bmb_bench_dim3(m)) { /* GCOVR_EXCL_BR_LINE: only a setup that ignores -K takes it; seen to, mutated */
        snprintf(msg, size, "the call's K is %zu, where the point's is %zu", inner, bmb_bench_dim3(m));
        return 1;
    }
    r = malloc((p * n + (p + 1) * inner + (4 * p + 2) * m) * sizeof(double));
    if (r == NULL) {
        return -1;
    }
    double *t = r + p * n, *tabs = t + p * inner;
    double *y1 = tabs + inner, *y1abs = y1 + p * m, *y2 = y1abs + m, *y2abs = y2 + p * m;
    double *want = y2abs + m, *scale = want + p * m;

    bmb_verify_probe(r, p * n);
    call(ctx);
    bmb_perturb(&ctx->c[0]);
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, y1, y1abs);
    bmb_matvec(BMB_VERIFY_OP(ctx->tb), ctx->col, (size_t) ctx->rb, (size_t) ctx->cb, ctx->b,
               (size_t) ctx->ldb, r, NULL, t, tabs);
    bmb_matvec(BMB_VERIFY_OP(ctx->ta), ctx->col, (size_t) ctx->ra, (size_t) ctx->ca, ctx->a,
               (size_t) ctx->lda, t, tabs, y2, y2abs);
    bmb_scale(want, ctx->alpha, y2, m);
    for (i = 0; i < p * m; i++) {
        scale[i] = bmb_abs(ctx->alpha) * y2abs[i / p];
#if !BMB_SINGLE
        scale[i] += y1abs[i / p];
#endif
    }
    ok = bmb_verify_close("C*r, for a random vector r" BMB_PARTS_NOTE, y1, want, scale, p * m,
                          bmb_tolerance(BMB_SINGLE ? inner : inner + n), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(gemm);
    bench.dim1_label = "Matrix dim1 (M=K)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.dim3_label = "Matrix dim3 (K)";
    bench.dim1_label_dim3 = "Matrix dim1 (M)";
    bench.use_vector_range = 0;
    bench.level = 3;
    bench.args = BMB_ARG_TRANS2;
    bench.trans_letters = BMB_TRANS_LETTERS;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
