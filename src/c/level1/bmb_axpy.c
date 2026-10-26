#include <cblas.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* saxpy, daxpy, caxpy and zaxpy, one per precision (bmb_prec.h): y +=
 * alpha * x. */
typedef struct {
    int n;
    bmb_real_t alpha[BMB_PARTS];
    bmb_real_t *x;
    bmb_real_t *y;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;

    (void) dim2;
    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) dim1;
    /* Small, so that repeated calls (Y += alpha*X) never over/underflow,
     * regardless of --iterations. */
    bmb_set_scalar(ctx->alpha, BMB_R(1.0e-6), BMB_R(1.0e-6));
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

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(axpy)(ctx->n, BMB_SCALAR(ctx->alpha), (const void *) ctx->x, 1, (void *) ctx->y, 1);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return BMB_MADD_FLOPS * d1;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return 3.0 * BMB_ELEM_BYTES * d1;
}

/* --verify: y0 + alpha * x, element by element, in double. */
static int verify_at(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    const bmb_real_t *al = ctx->alpha;
    size_t n = (size_t) ctx->n, i;
    double *y0 = malloc(5 * p * n * sizeof(double));
    int ok;

    if (y0 == NULL) {
        return -1;
    }
    double *got = y0 + p * n, *want = got + p * n, *scale = want + p * n, *x = scale + p * n;

    bmb_to_double(y0, ctx->y, n);
    bmb_to_double(x, ctx->x, n);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->y[0]);
    bmb_to_double(got, ctx->y, n);
    for (i = 0; i < n; i++) {
#if BMB_COMPLEX
        const double xr = x[2 * i], xi = x[2 * i + 1];
        const double axr = al[0] * xr - al[1] * xi, axi = al[0] * xi + al[1] * xr;

        want[2 * i] = y0[2 * i] + axr;
        want[2 * i + 1] = y0[2 * i + 1] + axi;
        scale[2 * i] = scale[2 * i + 1] = fabs(y0[2 * i]) + fabs(y0[2 * i + 1])
                                          + bmb_abs(al) * (fabs(xr) + fabs(xi));
#else
        const double ax = (double) al[0] * x[i];

        want[i] = y0[i] + ax;
        scale[i] = fabs(y0[i]) + fabs(ax);
#endif
    }
    ok = bmb_verify_close("y" BMB_PARTS_NOTE, got, want, scale, p * n, bmb_tolerance(1), msg, size);
    free(y0);
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

    bench.routine_name = BMB_ROUTINE(axpy);
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.level = 1;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
