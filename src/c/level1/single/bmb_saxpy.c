#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

typedef struct {
    int n;
    float alpha;
    float *x;
    float *y;
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
    /* Small, as for daxpy, so repeated calls (Y += alpha*X) never
     * over/underflow, regardless of --iterations. */
    ctx->alpha = 1.0e-6f;
    ctx->x = malloc(dim1 * sizeof(float));
    ctx->y = malloc(dim1 * sizeof(float));
    if (ctx->x == NULL || ctx->y == NULL) {
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < dim1; i++) {
        ctx->x[i] = (float) (i % 100) * 0.01f - 0.5f;
        ctx->y[i] = (float) ((i + 7) % 100) * 0.01f - 0.5f;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_saxpy(ctx->n, ctx->alpha, ctx->x, 1, ctx->y, 1);
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

    return 2.0 * d1;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;

    return 12.0 * d1;
}

/* --verify: y0 + alpha * x, element by element, in double. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n, i;
    double *y0 = malloc(4 * n * sizeof(double));
    int ok;

    if (y0 == NULL) {
        return -1;
    }
    double *got = y0 + n, *want = y0 + 2 * n, *scale = y0 + 3 * n;

    for (i = 0; i < n; i++) {
        y0[i] = ctx->y[i];
    }
    call(ctx);
    bmb_verify_perturb_single(&ctx->y[0]);
    for (i = 0; i < n; i++) {
        double ax = (double) ctx->alpha * ctx->x[i];

        got[i] = ctx->y[i];
        want[i] = y0[i] + ax;
        scale[i] = fabs(y0[i]) + fabs(ax);
    }
    ok = bmb_verify_close("y", got, want, scale, n, bmb_verify_tolerance_single(1), msg, size);
    free(y0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "saxpy";
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
