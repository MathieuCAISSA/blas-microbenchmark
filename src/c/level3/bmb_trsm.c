#include <cblas.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* strsm, dtrsm, ctrsm and ztrsm, one per precision (bmb_prec.h).
 * B is M x N, multiplied by the triangular A on the side --side gives
 * (#222): A is M x M on the left, the default, N x N on the right; its
 * triangle, transpose and diagonal as --uplo, --trans and --diag give
 * them. dim1 = M, dim2 = N. B is overwritten in place by the call; it is restored from b0
 * before every call so the benchmark is stable across --iterations. */
typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    int lda;
    int ldb;
    int m;
    int n;
    int na; /* A's order: M on the left, N on the right */
    char side, uplo, trans, diag;
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t *a;  /* na x na, the triangle --uplo gives used, diagonally dominant */
    bmb_real_t *b0; /* template */
    bmb_real_t *b;  /* working buffer, restored by reset() before each call */
} bmb_ctx_t;

/* Off the diagonal, small enough to keep the matrix diagonally dominant
 * (at most 0.5 / n, each part of a complex element half that), and
 * neither symmetric nor Hermitian: with a constant there, the upper
 * triangle read row-major equals the one read column-major, and --verify
 * could not tell a library that ignored --layout, or conjugated A. */
static bmb_real_t bmb_off_diagonal(size_t i, size_t j, size_t n)
{
    return (bmb_real_t) ((i * 7 + j * 3) % 11 + 1) / (BMB_R(24.0) * BMB_PARTS * (bmb_real_t) n);
}

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2;
    const int col = bmb_bench_column_major();
    /* B as stored: its rows, or its columns with --layout col. */
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

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->na = (int) na;
    ctx->side = bmb_bench_side();
    ctx->uplo = bmb_bench_uplo();
    ctx->trans = bmb_bench_trans(0);
    ctx->diag = bmb_bench_diag();
    bmb_set_scalar(ctx->alpha, BMB_R(1.0), BMB_R(0.0));
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (int) lda;
    ctx->ldb = (int) ldb;
    ctx->a = bmb_bench_alloc(na * lda * BMB_ELEM_BYTES);
    ctx->b0 = bmb_bench_alloc(outb * ldb * BMB_ELEM_BYTES);
    ctx->b = bmb_bench_alloc(outb * ldb * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->b0 == NULL || ctx->b == NULL) {
        free(ctx->a);
        free(ctx->b0);
        free(ctx->b);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < na; i++) {
        size_t j;

        /* Through the padding too, so that no element of A is left
         * uninitialised. */
        for (j = 0; j < lda; j++) {
            bmb_real_t *e = &ctx->a[BMB_PARTS * (i * lda + j)];

            e[0] = (i == j) ? (bmb_real_t) (m + 1) : bmb_off_diagonal(i, j, na);
#if BMB_COMPLEX
            e[1] = (i == j) ? BMB_R(0.5) : bmb_off_diagonal(j + 5, i, na);
#endif
        }
    }
    /* Away from zero, so that the solution stays of the size of B. */
    for (i = 0; i < outb * ldb; i++) {
        ctx->b0[BMB_PARTS * i] = (bmb_real_t) (i % 100) * BMB_R(0.01) + BMB_R(1.0);
#if BMB_COMPLEX
        ctx->b0[2 * i + 1] = (bmb_real_t) ((i + 37) % 100) * BMB_R(0.01) - BMB_R(0.5);
#endif
    }
    memcpy(ctx->b, ctx->b0, outb * ldb * BMB_ELEM_BYTES);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(trsm)(ctx->order, BMB_CBLAS_SIDE(ctx->side), BMB_CBLAS_UPLO(ctx->uplo),
                    BMB_CBLAS_TRANS(ctx->trans), BMB_CBLAS_DIAG(ctx->diag),
                    ctx->m, ctx->n, BMB_SCALAR(ctx->alpha), (const void *) ctx->a, ctx->lda,
                    (void *) ctx->b, ctx->ldb);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->b, ctx->b0, (size_t) (ctx->col ? ctx->n : ctx->m) * (size_t) ctx->ldb * BMB_ELEM_BYTES);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b0);
    free(ctx->b);
    free(ctx);
}

/* N*M*(M+1) on the left, M*N*(N+1) on the right: A's order (M+1) times
 * B's size. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    if (bmb_bench_side() == 'R') {
        return BMB_FLOP_FACTOR * d1 * d2 * (d2 + 1.0);
    }
    return BMB_FLOP_FACTOR * d2 * d1 * (d1 + 1.0);
}

/* --cache cold: the bytes of A and B: M x M (N x N on the right), M x N; the copy reset() restores B from is not touched by the call. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;

    const double na = bmb_bench_side() == 'L' ? d1 : d2;

    return (na * na + d1 * d2) * (double) (BMB_ELEM_BYTES);
}

/* --verify, by random projection of the residual, in double: op(A)*(X*r)
 * on the left, X*(op(A)*r) on the right, against alpha*B0*r, r of the
 * routine's kind, X being the solution the call left in B and B0 the
 * template the driver has just restored B from. In single precision, the
 * library rounded each element of X through a substitution of as many
 * terms as A's order, which is what the tolerance counts. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, na = (size_t) ctx->na, i;
    const int shape = BMB_VERIFY_TRI(ctx->uplo, ctx->trans, ctx->diag);
    double *r = malloc((p * n + (p + 1) * na + (4 * p + 2) * m) * sizeof(double));
    int ok;

    if (r == NULL) {
        return -1;
    }
    double *t = r + p * n, *tabs = t + p * na, *y1 = tabs + na, *y1abs = y1 + p * m;
    double *u = y1abs + m, *uabs = u + p * m, *want = uabs + m, *scale = want + p * m;

    bmb_verify_probe(r, p * n);
    call(ctx);
    bmb_perturb(&ctx->b[0]);
    if (ctx->side == 'L') {
        /* op(A) * X = alpha * B0: op(A)*(X*r) against alpha*B0*r. */
        bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b, (size_t) ctx->ldb, r, NULL, t, tabs);
        bmb_matvec(shape, ctx->col, m, m, ctx->a, (size_t) ctx->lda, t, tabs, y1, y1abs);
    } else {
        /* X * op(A) = alpha * B0: X*(op(A)*r) against alpha*B0*r. */
        bmb_matvec(shape, ctx->col, n, n, ctx->a, (size_t) ctx->lda, r, NULL, t, tabs);
        bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b, (size_t) ctx->ldb, t, tabs, y1, y1abs);
    }
    bmb_matvec(BMB_VERIFY_FULL, ctx->col, m, n, ctx->b0, (size_t) ctx->ldb, r, NULL, u, uabs);
    bmb_scale(want, ctx->alpha, u, m);
    for (i = 0; i < p * m; i++) {
        scale[i] = y1abs[i / p] + bmb_abs(ctx->alpha) * uabs[i / p];
    }
    ok = bmb_verify_close(ctx->side == 'R' ? "X*(op(A)*r), the residual for a random vector r" BMB_PARTS_NOTE
                          : ctx->uplo == 'U' && ctx->trans == 'N'
                              ? "U*(X*r), the residual for a random vector r" BMB_PARTS_NOTE
                              : "op(A)*(X*r), the residual for a random vector r" BMB_PARTS_NOTE,
                          y1, want, scale, p * m, bmb_tolerance(BMB_SINGLE ? na : na + n), msg, size);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(trsm);
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.level = 3;
    bench.args = BMB_ARG_SIDE | BMB_ARG_UPLO | BMB_ARG_TRANS | BMB_ARG_DIAG;
    bench.trans_letters = BMB_TRANS_LETTERS;
    bench.setup = setup;
    bench.call = call;
    bench.reset = reset;
    bench.reset_every_call = 1;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
