#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* sswap, dswap, cswap and zswap, one per precision (bmb_prec.h): x and y
 * exchanged. */
typedef struct {
    int n;
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

    BMB_CBLAS(swap)(ctx->n, (void *) ctx->x, 1, (void *) ctx->y, 1);
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
    return 4.0 * BMB_ELEM_BYTES * d1;
}

/* --verify: x and y are now exactly each other's. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const size_t p = BMB_PARTS;
    size_t n = (size_t) ctx->n;
    double *x0 = malloc(4 * p * n * sizeof(double));
    int ok;

    if (x0 == NULL) {
        return -1;
    }
    double *y0 = x0 + p * n, *x = y0 + p * n, *y = x + p * n;

    bmb_to_double(x0, ctx->x, n);
    bmb_to_double(y0, ctx->y, n);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->x[0]);
    bmb_to_double(x, ctx->x, n);
    bmb_to_double(y, ctx->y, n);
    /* Both checked, whichever fails: & rather than &&. */
    ok = bmb_verify_close("x" BMB_PARTS_NOTE, x, y0, NULL, p * n, 0.0, msg, size)
         & bmb_verify_close("y" BMB_PARTS_NOTE, y, x0, NULL, p * n, 0.0, msg, size);
    free(x0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(swap);
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
