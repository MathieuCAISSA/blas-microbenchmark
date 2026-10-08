#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

typedef struct {
    int n;
    float *x;
    float *y;
    volatile float sink;
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

    ctx->sink = cblas_sdot(ctx->n, ctx->x, 1, ctx->y, 1);
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

    return 8.0 * d1;
}

/* --verify: the dot product, recomputed in double. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    double got, want = 0.0, scale = 0.0;
    float result;
    int i;

    call(ctx);
    result = ctx->sink;
    bmb_verify_perturb_single(&result);
    got = result;
    for (i = 0; i < ctx->n; i++) {
        double p = (double) ctx->x[i] * ctx->y[i];

        want += p;
        scale += fabs(p);
    }
    return !bmb_verify_close("the dot product", &got, &want, &scale, 1,
                             bmb_verify_tolerance_single((size_t) ctx->n), msg, size);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "sdot";
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
