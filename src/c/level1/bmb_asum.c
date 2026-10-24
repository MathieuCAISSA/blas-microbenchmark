#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sasum, dasum, scasum and dzasum, one per precision (bmb_prec.h): the sum of |x|, |re| + |im| for a complex element.
 * The result is real for a complex vector too, which BLAS names for both
 * precisions (BMB_CBLAS_REAL). */
typedef struct {
    int n;
    bmb_real_t *x;
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
    if (ctx->x == NULL) {
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->x, dim1, 0, 31);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    ctx->sink = BMB_CBLAS_REAL(asum)(ctx->n, (const void *) ctx->x, 1);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x);
    free(ctx);
}

static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return BMB_PARTS * d1;
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (double) BMB_ELEM_BYTES * d1;
}

/* --verify: sum, recomputed in double. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    bmb_real_t result;
    double got, want, sum = 0.0;
    int i;

    call(ctx);
    result = ctx->sink;
    bmb_perturb(&result);
    got = result;
    for (i = 0; i < ctx->n; i++) {
#if BMB_COMPLEX
        const double re = ctx->x[2 * i], im = ctx->x[2 * i + 1];

        sum += fabs(re) + fabs(im);
#else
        const double xi = ctx->x[i];

        sum += fabs(xi);
#endif
    }
    want = sum;
    return !bmb_verify_close("the sum", &got, &want, &want, 1,
                             bmb_tolerance((size_t) ctx->n), msg, size);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE_REAL(asum);
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
