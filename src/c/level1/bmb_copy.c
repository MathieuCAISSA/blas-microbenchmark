#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* scopy, dcopy, ccopy and zcopy, one per precision (bmb_prec.h): y = x. */
typedef struct {
    int n;
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
    for (i = 0; i < BMB_PARTS * dim1; i++) {
        ctx->y[i] = BMB_R(0.0);
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(copy)(ctx->n, (const void *) ctx->x, 1, (void *) ctx->y, 1);
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
    return 2.0 * BMB_ELEM_BYTES * d1;
}

/* --verify: y is now exactly x. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t n = (size_t) ctx->n;
    double *got = malloc(2 * p * n * sizeof(double));
    int ok;

    if (got == NULL) {
        return -1;
    }
    double *x = got + p * n;

    call(ctx);
    bmb_perturb(&ctx->y[0]);
    bmb_to_double(got, ctx->y, n);
    bmb_to_double(x, ctx->x, n);
    ok = bmb_verify_close("y" BMB_PARTS_NOTE, got, x, NULL, p * n, 0.0, msg, size);
    free(got);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(copy);
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.level = 1;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.verify = verify;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
