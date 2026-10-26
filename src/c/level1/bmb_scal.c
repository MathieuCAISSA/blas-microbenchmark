#include <cblas.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sscal, dscal, cscal and zscal, one per precision (bmb_prec.h): x =
 * alpha * x, x restored from x0 before every timed batch. */
typedef struct {
    int n;
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t *x0; /* template */
    bmb_real_t *x;  /* working buffer, restored by reset() before each call */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t i;

    (void) dim2;
    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) dim1;
    /* Deliberately not exactly 1.0: some BLAS implementations special-case
     * alpha == 1 as a no-op fast path, which would not reflect real work.
     * Of modulus below 1, complex or not. */
    bmb_set_scalar(ctx->alpha, BMB_R(0.999999), BMB_R(1.0e-6));
    ctx->x0 = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    if (ctx->x0 == NULL || ctx->x == NULL) {
        free(ctx->x0);
        free(ctx->x);
        free(ctx);
        return NULL;
    }

    /* Away from zero, so that no element is scaled into a denormal. */
    for (i = 0; i < dim1; i++) {
        ctx->x0[BMB_PARTS * i] = (bmb_real_t) (i % 100) * BMB_R(0.01) + BMB_R(1.0);
#if BMB_COMPLEX
        ctx->x0[2 * i + 1] = (bmb_real_t) ((i + 37) % 100) * BMB_R(0.01) - BMB_R(0.5);
#endif
    }
    memcpy(ctx->x, ctx->x0, dim1 * BMB_ELEM_BYTES);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(scal)(ctx->n, BMB_SCALAR(ctx->alpha), (void *) ctx->x, 1);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->x, ctx->x0, (size_t) ctx->n * BMB_ELEM_BYTES);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x0);
    free(ctx->x);
    free(ctx);
}

/* A complex alpha times a complex x is 6 real operations. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (BMB_COMPLEX ? 6.0 : 1.0) * d1;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return 2.0 * BMB_ELEM_BYTES * d1;
}

/* --verify: alpha times the template, element by element, in double. The
 * driver has just restored x from it. */
static int verify_at(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t n = (size_t) ctx->n, i;
    double *want = malloc(4 * p * n * sizeof(double));
    int ok;

    if (want == NULL) {
        return -1;
    }
    double *scale = want + p * n, *x0 = scale + p * n, *got = x0 + p * n;

    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->x[0]);
    bmb_to_double(x0, ctx->x0, n);
    bmb_to_double(got, ctx->x, n);
    bmb_scale(want, ctx->alpha, x0, n);
    for (i = 0; i < n; i++) {
#if BMB_COMPLEX
        scale[2 * i] = scale[2 * i + 1] = bmb_abs(ctx->alpha) * (fabs(x0[2 * i]) + fabs(x0[2 * i + 1]));
#else
        scale[i] = fabs(want[i]);
#endif
    }
    ok = bmb_verify_close("x" BMB_PARTS_NOTE, got, want, scale, p * n, bmb_tolerance(1), msg, size);
    free(want);
    return !ok;
}

/* --verify makes its call with an alpha of order 1, then gives the timed
 * one back: the timed alpha is kept small so that repeated calls do not
 * drift, and its update would fall inside single precision's tolerance,
 * where a check could not tell it from no call at all (#239). */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    bmb_real_t timed[sizeof(ctx->alpha) / sizeof(ctx->alpha[0])];
    int rc;

    memcpy(timed, ctx->alpha, sizeof(timed));
    bmb_set_scalar(ctx->alpha, BMB_R(0.5), BMB_R(0.25));
    rc = verify_at(vctx, msg, size);
    memcpy(ctx->alpha, timed, sizeof(timed));
    return rc;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(scal);
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.level = 1;
    bench.setup = setup;
    bench.call = call;
    bench.reset = reset;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
