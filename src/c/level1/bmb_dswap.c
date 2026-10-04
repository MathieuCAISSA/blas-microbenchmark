#include <cblas.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

typedef struct {
    int n;
    double *x;
    double *y;
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
    ctx->x = malloc(dim1 * sizeof(double));
    ctx->y = malloc(dim1 * sizeof(double));
    if (ctx->x == NULL || ctx->y == NULL) {
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < dim1; i++) {
        ctx->x[i] = (double) (i % 100) * 0.01 - 0.5;
        ctx->y[i] = (double) ((i + 7) % 100) * 0.01 - 0.5;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_dswap(ctx->n, ctx->x, 1, ctx->y, 1);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;

    return 32.0 * d1;
}

/* --verify: x and y are now exactly each other's. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    size_t n = (size_t) ctx->n;
    double *x0 = malloc(2 * n * sizeof(double));
    int ok;

    if (x0 == NULL) {
        return -1;
    }
    double *y0 = x0 + n;

    memcpy(x0, ctx->x, n * sizeof(double));
    memcpy(y0, ctx->y, n * sizeof(double));
    call(ctx);
    bmb_verify_perturb(&ctx->x[0]);
    ok = bmb_verify_close("x", ctx->x, y0, NULL, n, 0.0, msg, size)
         && bmb_verify_close("y", ctx->y, x0, NULL, n, 0.0, msg, size);
    free(x0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "dswap";
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
