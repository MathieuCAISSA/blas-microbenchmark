#include <cblas.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* saxpby, daxpby, caxpby and zaxpby, one per precision (bmb_prec.h): y =
 * alpha * x + beta * y, an extension of the BLAS (#171) that OpenBLAS,
 * MKL, BLIS and AOCL-BLAS have; a build whose library does not is
 * skipped. */
typedef struct {
    bmb_int n;
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t beta[BMB_PARTS];
    bmb_real_t *x;
    bmb_real_t *y;
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

    ctx->n = (bmb_int) dim1;
    /* Repeated calls take y towards alpha / (1 - beta) * x, a third of x
     * here, whatever --iterations: no element of x is 0 (below), where y
     * would be taken to 0 through the denormals. A beta of exactly 1 would
     * let a library call axpy instead. */
    bmb_set_scalar(ctx->alpha, BMB_R(0.25), BMB_R(0.0));
    bmb_set_scalar(ctx->beta, BMB_R(0.25), BMB_R(0.0));
    ctx->x = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    ctx->y = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    if (ctx->x == NULL || ctx->y == NULL) {
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->x, dim1, 0, 31);
    bmb_fill(ctx->y, dim1, 7, 53);
    /* Half a step off the fill's grid, which has a 0 in every hundred. */
    for (i = 0; i < BMB_PARTS * dim1; i++) {
        ctx->x[i] += BMB_R(0.005);
    }

    return ctx;
}

static void call(void *vctx)
{
#if defined(HAVE_CBLAS_AXPBY)
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(axpby)(ctx->n, BMB_SCALAR(ctx->alpha), (const void *) ctx->x, 1,
                     BMB_SCALAR(ctx->beta), (void *) ctx->y, 1);
#else
    (void) vctx;
#endif
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

/* Two products and a sum an element: 3 operations real, 6 + 6 + 2
 * complex. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (BMB_COMPLEX ? 14.0 : 3.0) * d1;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return 3.0 * BMB_ELEM_BYTES * d1;
}

/* --verify: alpha * x + beta * y0, element by element, in double, with
 * an alpha and a beta of their own, complex where the routine is: the
 * timed ones, real, would not show a library that dropped an imaginary
 * part. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    bmb_real_t timed_alpha[BMB_PARTS], timed_beta[BMB_PARTS];
    const bmb_real_t *al = ctx->alpha, *be = ctx->beta;
    size_t n = (size_t) ctx->n, i;
    double *y0 = malloc(5 * p * n * sizeof(double));
    int ok;

    if (y0 == NULL) {
        return -1;
    }
    double *got = y0 + p * n, *want = got + p * n, *scale = want + p * n, *x = scale + p * n;

    memcpy(timed_alpha, ctx->alpha, sizeof(timed_alpha));
    memcpy(timed_beta, ctx->beta, sizeof(timed_beta));
    bmb_set_scalar(ctx->alpha, BMB_R(0.5), BMB_R(0.25));
    bmb_set_scalar(ctx->beta, BMB_R(0.75), BMB_R(-0.5));
    bmb_to_double(y0, ctx->y, n);
    bmb_to_double(x, ctx->x, n);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->y[0]);
    bmb_to_double(got, ctx->y, n);
    for (i = 0; i < n; i++) {
#if BMB_COMPLEX
        const double xr = x[2 * i], xi = x[2 * i + 1], yr = y0[2 * i], yi = y0[2 * i + 1];

        want[2 * i] = al[0] * xr - al[1] * xi + be[0] * yr - be[1] * yi;
        want[2 * i + 1] = al[0] * xi + al[1] * xr + be[0] * yi + be[1] * yr;
        scale[2 * i] = scale[2 * i + 1] = bmb_abs(al) * (fabs(xr) + fabs(xi))
                                          + bmb_abs(be) * (fabs(yr) + fabs(yi));
#else
        want[i] = (double) al[0] * x[i] + (double) be[0] * y0[i];
        scale[i] = fabs((double) al[0] * x[i]) + fabs((double) be[0] * y0[i]);
#endif
    }
    ok = bmb_verify_close("y" BMB_PARTS_NOTE, got, want, scale, p * n, bmb_tolerance(1), msg, size);
    memcpy(ctx->alpha, timed_alpha, sizeof(timed_alpha));
    memcpy(ctx->beta, timed_beta, sizeof(timed_beta));
    free(y0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(axpby);
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.level = 1;
#if !defined(HAVE_CBLAS_AXPBY)
    bench.missing = "cblas_?axpby";
#endif
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
