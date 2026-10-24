#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sdot and ddot, one per precision (bmb_prec.h): x' * y. The complex
 * ones, dotc and dotu, are families of their own. */
#if BMB_COMPLEX
#error "dot is real: the complex ones are dotc and dotu"
#endif

typedef struct {
    int n;
    bmb_real_t *x;
    bmb_real_t *y;
    volatile bmb_real_t sink;
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

    ctx->sink = BMB_CBLAS(dot)(ctx->n, ctx->x, 1, ctx->y, 1);
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
    return 2.0 * BMB_ELEM_BYTES * d1;
}

/* --verify: the dot product, recomputed in double. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    bmb_real_t result;
    double got, want = 0.0, scale = 0.0;
    int i;

    call(ctx);
    result = ctx->sink;
    bmb_perturb(&result);
    got = result;
    for (i = 0; i < ctx->n; i++) {
        const double p = (double) ctx->x[i] * ctx->y[i];

        want += p;
        scale += fabs(p);
    }
    return !bmb_verify_close("the dot product", &got, &want, &scale, 1,
                             bmb_tolerance((size_t) ctx->n), msg, size);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(dot);
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
