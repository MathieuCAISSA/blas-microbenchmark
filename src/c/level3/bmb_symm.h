/* The source of symm and of hemm, which bmb_symm.c and bmb_hemm.c
 * compile, the second with BMB_HERM defined (#209): a header, so that
 * the two programs share it without one source including another. */
#ifndef BMB_SYMM_H
#define BMB_SYMM_H

#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* ssymm, dsymm, csymm and zsymm, one per precision (bmb_prec.h); with
 * BMB_HERM defined, chemm and zhemm (bmb_hemm.c), whose A is Hermitian
 * where symm's is symmetric, and nothing else changes.
 * B and C are M x N, multiplied by A on the side --side gives (#222): A
 * is M x M on the left, the default, N x N on the right, stored in the
 * triangle --uplo gives. dim1 = M, dim2 = N. A
 * complex A's diagonal has an imaginary part, which a Hermitian matrix
 * cannot have and BLAS does not read: --verify ignores it too, so a hemm
 * that used it would be caught. */
#ifdef BMB_HERM
#define BMB_OP hemm
#define BMB_SHAPE(uplo) ((uplo) == 'U' ? BMB_VERIFY_HERM_UPPER : BMB_VERIFY_HERM_LOWER)
#else
#define BMB_OP symm
#define BMB_SHAPE(uplo) ((uplo) == 'U' ? BMB_VERIFY_SYM_UPPER : BMB_VERIFY_SYM_LOWER)
#endif

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldb; /* B's and C's: they have the same shape */
    int m;
    int n;
    int na; /* A's order: M on the left, N on the right */
    char side, uplo;
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t beta[BMB_PARTS];
    bmb_real_t *a; /* na x na, the triangle --uplo gives used */
    bmb_real_t *b; /* M x N */
    bmb_real_t *c; /* M x N */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2;
    const int col = bmb_bench_column_major();
    /* B and C as stored: their rows, or their columns with --layout col. */
    const size_t outb = col ? n : m;
    const size_t na = bmb_bench_side() == 'L' ? m : n;
    const size_t lda = bmb_bench_ld(na, na, BMB_ELEM_BYTES);
    const size_t ldb = bmb_bench_ld(outb, col ? m : n, BMB_ELEM_BYTES);
    size_t i;

    (void) thread_count;

#if BMB_ELEM_BYTES > 8
    if (lda == 0 || ldb == 0) {
        bmb_bench_too_large();
        return NULL;
    }
#endif
    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->na = (int) na;
    ctx->side = bmb_bench_side();
    ctx->uplo = bmb_bench_uplo();
    ctx->m = (int) m;
    ctx->n = (int) n;
    bmb_set_scalar(ctx->alpha, BMB_R(1.0), BMB_R(0.0));
    bmb_set_scalar(ctx->beta, BMB_R(0.0), BMB_R(0.0)); /* verify() relies on it */
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->ldb = (int) ldb;
    ctx->a = bmb_bench_alloc(na * lda * BMB_ELEM_BYTES);
    ctx->b = bmb_bench_alloc(outb * ldb * BMB_ELEM_BYTES);
    ctx->c = bmb_bench_alloc(outb * ldb * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->a, na * lda, 0, 31);
    bmb_fill(ctx->b, outb * ldb, 7, 53);
    for (i = 0; i < BMB_PARTS * outb * ldb; i++) {
        ctx->c[i] = BMB_R(0.0);
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(BMB_OP)(ctx->order, BMB_CBLAS_SIDE(ctx->side), BMB_CBLAS_UPLO(ctx->uplo), ctx->m, ctx->n,
                     BMB_SCALAR(ctx->alpha), (const void *) ctx->a, ctx->lda,
                     (const void *) ctx->b, ctx->ldb, BMB_SCALAR(ctx->beta),
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

/* 2*M*M*N on the left, 2*M*N*N on the right. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    if (bmb_bench_side() == 'R') {
        return BMB_MADD_FLOPS * d1 * d2 * d2;
    }
    return BMB_MADD_FLOPS * d1 * d1 * d2;
}

/* --cache cold: the bytes of A, B and C: M x M (N x N on the right),
 * M x N, M x N. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;
    const double na = bmb_bench_side() == 'L' ? d1 : d2;

    return (na * na + 2.0 * d1 * d2) * (double) (BMB_ELEM_BYTES);
}

/* --verify, by random projection in double: C*r against alpha*A*(B*r) on
 * the left, alpha*B*(A*r) on the right, r of the routine's kind, A
 * symmetric or Hermitian. beta is 0 (setup), so
 * what C held before the call does not enter. In single precision, the
 * scale leaves out |C||r| and the tolerance counts the library's M terms
 * alone, as for sgemm. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, na = (size_t) ctx->na, i;
    double *r = malloc((p * n + (p + 1) * na + (4 * p + 2) * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + p * n, *tabs = t + p * na;
    double *y1 = tabs + na, *y1abs = y1 + p * m, *y2 = y1abs + m, *y2abs = y2 + p * m;
    double *want = y2abs + m, *scale = want + p * m;

    bmb_verify_probe(r, p * n);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->c[0]);
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->c, (size_t) ctx->ldb, r, NULL, y1, y1abs);
    if (ctx->side == 'L') {
        bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b, (size_t) ctx->ldb, r, NULL, t, tabs);
        bmb_matvec(BMB_SHAPE(ctx->uplo), ctx->col, m, m, ctx->a, (size_t) ctx->lda, t, tabs, y2, y2abs);
    } else {
        bmb_matvec(BMB_SHAPE(ctx->uplo), ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, t, tabs);
        bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b, (size_t) ctx->ldb, t, tabs, y2, y2abs);
    }
    bmb_scale(want, ctx->alpha, y2, m);
    for (i = 0; i < p * m; i++) {
        scale[i] = bmb_abs(ctx->alpha) * y2abs[i / p];
#if !BMB_SINGLE
        scale[i] += y1abs[i / p];
#endif
    }
    ok = bmb_verify_close("C*r, for a random vector r" BMB_PARTS_NOTE, y1, want, scale, p * m,
                          bmb_tolerance(BMB_SINGLE ? na : na + n), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(BMB_OP);
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.level = 3;
    bench.args = BMB_ARG_SIDE | BMB_ARG_UPLO;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}

#endif /* BMB_SYMM_H */
