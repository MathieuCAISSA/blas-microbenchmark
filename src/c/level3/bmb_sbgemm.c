#include <config.h>

#include <cblas.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sbgemm (#322): C = alpha * op(A) * op(B) + beta * C, A and B in
 * bfloat16, C and the accumulation in float, as machine-learning and
 * mixed-precision codes call it: OpenBLAS's cblas_sbgemm (built with
 * BUILD_BFLOAT16), MKL's cblas_gemm_bf16bf16f32, which take the same
 * arguments. An extension of the BLAS (#171); a build whose library has
 * neither is skipped. Compiled as single precision (bmb_prec.h), C's.
 * dim1 = M, dim2 = N, K apart under -K, as GEMM's. */
#if defined(HAVE_CBLAS_GEMM_BF16BF16F32)
#define BMB_BF16_GEMM cblas_gemm_bf16bf16f32
#else
#define BMB_BF16_GEMM cblas_sbgemm
#endif

/* bfloat16: a float's upper 16 bits, 8 of significand. */
#define BMB_BF16_BYTES 2

typedef struct {
    int order; /* CblasRowMajor, or CblasColMajor with --layout col */
    int col;   /* 1 with --layout col */
    bmb_int lda;
    bmb_int ldb;
    bmb_int ldc;
    bmb_int m;
    bmb_int n;
    bmb_int k;
    char ta, tb; /* --trans, A's and B's */
    bmb_int ra, ca, rb, cb; /* A's and B's rows and columns, as stored */
    size_t sa, sb;          /* A's and B's elements, leading dimensions in */
    float alpha;
    float beta;
    uint16_t *a; /* M x K, or K x M transposed, in bfloat16 */
    uint16_t *b; /* K x N, or N x K transposed */
    float *c;    /* M x N */
} bmb_ctx_t;

/* A float rounded to the nearest bfloat16, ties to even. The fill has no
 * NaN or infinity, which this would not keep. */
static uint16_t bmb_to_bf16(float f)
{
    uint32_t u;

    memcpy(&u, &f, sizeof(u));
    u += 0x7fffu + ((u >> 16) & 1u);
    return (uint16_t) (u >> 16);
}

static float bmb_from_bf16(uint16_t h)
{
    const uint32_t u = (uint32_t) h << 16;
    float f;

    memcpy(&f, &u, sizeof(f));
    return f;
}

/* count elements of the fill every routine uses (bmb_fill), offset by
 * shift, rounded to bfloat16. */
static void bmb_fill_bf16(uint16_t *x, size_t count, size_t shift)
{
    size_t i;

    for (i = 0; i < count; i++) {
        x[i] = bmb_to_bf16((float) ((i + shift) % 100) * 0.01f - 0.5f);
    }
}

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2, k = bmb_bench_dim3(dim1);
    const int col = bmb_bench_column_major();
    const char ta = bmb_bench_trans(0), tb = bmb_bench_trans(1);
    const size_t ra = ta == 'N' ? m : k, ca = ta == 'N' ? k : m;
    const size_t rb = tb == 'N' ? k : n, cb = tb == 'N' ? n : k;
    const size_t outa = col ? ca : ra, outb = col ? cb : rb, outc = col ? n : m;
    const size_t lda = bmb_bench_ld(outa, col ? ra : ca, BMB_BF16_BYTES);
    const size_t ldb = bmb_bench_ld(outb, col ? rb : cb, BMB_BF16_BYTES);
    const size_t ldc = bmb_bench_ld(outc, col ? m : n, BMB_ELEM_BYTES);
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (bmb_int) m;
    ctx->n = (bmb_int) n;
    ctx->k = (bmb_int) k;
    ctx->ta = ta;
    ctx->tb = tb;
    ctx->ra = (bmb_int) ra;
    ctx->ca = (bmb_int) ca;
    ctx->rb = (bmb_int) rb;
    ctx->cb = (bmb_int) cb;
    ctx->sa = outa * lda;
    ctx->sb = outb * ldb;
    ctx->alpha = 1.0f;
    ctx->beta = 0.0f; /* verify() relies on it */
    ctx->col = col;
    ctx->order = ctx->col ? CblasColMajor : CblasRowMajor;
    ctx->lda = (bmb_int) lda;
    ctx->ldb = (bmb_int) ldb;
    ctx->ldc = (bmb_int) ldc;
    ctx->a = bmb_bench_alloc(ctx->sa * BMB_BF16_BYTES);
    ctx->b = bmb_bench_alloc(ctx->sb * BMB_BF16_BYTES);
    ctx->c = bmb_bench_alloc(outc * ldc * BMB_ELEM_BYTES);
    if (ctx->a == NULL || ctx->b == NULL || ctx->c == NULL) {
        free(ctx->a);
        free(ctx->b);
        free(ctx->c);
        free(ctx);
        return NULL;
    }

    bmb_fill_bf16(ctx->a, ctx->sa, 0);
    bmb_fill_bf16(ctx->b, ctx->sb, 7);
    for (i = 0; i < outc * ldc; i++) {
        ctx->c[i] = 0.0f;
    }

    return ctx;
}

static void call(void *vctx)
{
#if defined(HAVE_CBLAS_SBGEMM)
    bmb_ctx_t *ctx = vctx;

    BMB_BF16_GEMM(ctx->order, BMB_CBLAS_TRANS(ctx->ta), BMB_CBLAS_TRANS(ctx->tb), ctx->m, ctx->n, ctx->k,
                  ctx->alpha, ctx->a, ctx->lda, ctx->b, ctx->ldb, ctx->beta, ctx->c, ctx->ldc);
#else
    (void) vctx;
#endif
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->a);
    free(ctx->b);
    free(ctx->c);
    free(ctx);
}

/* GEMM's 2 M N K, each a bfloat16 multiply and a float add. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;
    const double k = (double) bmb_bench_dim3(dim1);

    return 2.0 * d1 * k * d2;
}

/* --cache cold: A and B in bfloat16, C in float. */
static double footprint(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;
    const double d2 = (double) dim2;
    const double k = (double) bmb_bench_dim3(dim1);

    return (d1 * k + k * d2) * BMB_BF16_BYTES + d1 * d2 * (double) (BMB_ELEM_BYTES);
}

/* --verify, by random projection in double, as sgemm's: C*r against
 * alpha*op(A)*(op(B)*r), on the bfloat16 values the library was given,
 * copied to float, which holds them exactly. A product of two of them
 * has 16 bits of significand, which float holds exactly too: what is
 * left is the K float additions, single precision's tolerance for K
 * terms. A library that accumulated in bfloat16 would be off by 2^-8,
 * far past it. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t m = (size_t) ctx->m, n = (size_t) ctx->n, inner = (size_t) ctx->k, i;
    float *af;
    double *r;
    int ok;

    /* The call's K is the point's (-K), as GEMM's check requires: a setup
     * that kept K = M would pass the projection, which reads the K the
     * call was given. */
    if (inner != bmb_bench_dim3(m)) { /* GCOVR_EXCL_BR_LINE: only a setup that ignores -K takes it; seen to, mutated */
        /* GCOVR_EXCL_START: only a setup that ignores -K takes it; seen to, mutated */
        snprintf(msg, size, "the call's K is %zu, where the point's is %zu", inner, bmb_bench_dim3(m));
        return 1;
        /* GCOVR_EXCL_STOP */
    }
    af = malloc((ctx->sa + ctx->sb) * sizeof(float));
    r = malloc((n + 2 * inner + 6 * m) * sizeof(double));
    if (af == NULL || r == NULL) {
        free(af);
        free(r);
        return -1;
    }
    float *bf = af + ctx->sa;
    double *t = r + n, *tabs = t + inner;
    double *y1 = tabs + inner, *y1abs = y1 + m, *y2 = y1abs + m, *y2abs = y2 + m;
    double *want = y2abs + m, *scale = want + m;

    for (i = 0; i < ctx->sa; i++) {
        af[i] = bmb_from_bf16(ctx->a[i]);
    }
    for (i = 0; i < ctx->sb; i++) {
        bf[i] = bmb_from_bf16(ctx->b[i]);
    }
    bmb_verify_probe(r, n);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->c[0]);
    bmb_verify_matvec_single(BMB_VERIFY_FULL, ctx->col, m, n, ctx->c, (size_t) ctx->ldc, r, NULL, y1, y1abs);
    bmb_verify_matvec_single(BMB_VERIFY_OP(ctx->tb), ctx->col, (size_t) ctx->rb, (size_t) ctx->cb, bf,
                             (size_t) ctx->ldb, r, NULL, t, tabs);
    bmb_verify_matvec_single(BMB_VERIFY_OP(ctx->ta), ctx->col, (size_t) ctx->ra, (size_t) ctx->ca, af,
                             (size_t) ctx->lda, t, tabs, y2, y2abs);
    for (i = 0; i < m; i++) {
        want[i] = (double) ctx->alpha * y2[i];
        scale[i] = (double) ctx->alpha * y2abs[i];
    }
    ok = bmb_verify_close("C*r, for a random vector r", y1, want, scale, m,
                          bmb_verify_tolerance_single(inner), msg, size);
    free(af);
    free(r);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "sbgemm";
    bench.dim1_label = "Matrix dim1 (M=K)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.dim3_label = "Matrix dim3 (K)";
    bench.dim1_label_dim3 = "Matrix dim1 (M)";
    bench.use_vector_range = 0;
    bench.level = 3;
    bench.args = BMB_ARG_TRANS2;
    bench.trans_letters = "NT";
    bench.input_type = "bf16";
    bench.accumulator_type = "fp32";
#if !defined(HAVE_CBLAS_SBGEMM)
    bench.missing = "cblas_sbgemm or cblas_gemm_bf16bf16f32";
#endif
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.footprint = footprint;

    return bmb_benchmark_main(argc, argv, &bench);
}
