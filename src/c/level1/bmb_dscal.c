#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"

typedef struct {
    int n;
    double alpha;
    double *x;
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
     * alpha == 1 as a no-op fast path, which would not reflect real work. */
    ctx->alpha = 0.999999;
    ctx->x = malloc(dim1 * sizeof(double));
    if (ctx->x == NULL) {
        free(ctx);
        return NULL;
    }

    for (i = 0; i < dim1; i++) {
        ctx->x[i] = (double) (i % 100) * 0.01 + 1.0;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dscal(ctx->n, ctx->alpha, ctx->x, 1);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x);
    free(ctx);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dscal";
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;

    return bmb_benchmark_main(argc, argv, &bench);
}
