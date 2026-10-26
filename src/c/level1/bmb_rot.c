#include <cblas.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* srot and drot, one per precision (bmb_prec.h): the plane rotation
 * x' = c x + s y, y' = c y - s x, x and y restored from x0 and y0 before
 * every timed batch. The complex vectors' ones, csrot and zdrot, are not
 * in every CBLAS (BLIS 0.9's has neither). */
#if BMB_COMPLEX
#error "rot is real here: csrot and zdrot are not in every CBLAS"
#endif

typedef struct {
    int n;
    bmb_real_t c, s;
    bmb_real_t *x0, *y0; /* templates */
    bmb_real_t *x, *y;   /* working buffers, restored by reset() */
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
    /* A rotation keeps the vectors' norms, but c^2 + s^2 is 1 only to
     * rounding: 1 + 5e-8 in single precision, a growth that would take
     * billions of calls to overflow, and is reset with every batch. */
    ctx->c = BMB_R(0.6);
    ctx->s = BMB_R(0.8);
    ctx->x0 = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    ctx->y0 = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    ctx->x = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    ctx->y = bmb_bench_alloc(dim1 * BMB_ELEM_BYTES);
    if (ctx->x0 == NULL || ctx->y0 == NULL || ctx->x == NULL || ctx->y == NULL) {
        free(ctx->x0);
        free(ctx->y0);
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    bmb_fill(ctx->x0, dim1, 0, 31);
    bmb_fill(ctx->y0, dim1, 7, 53);
    memcpy(ctx->x, ctx->x0, dim1 * BMB_ELEM_BYTES);
    memcpy(ctx->y, ctx->y0, dim1 * BMB_ELEM_BYTES);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    BMB_CBLAS(rot)(ctx->n, ctx->x, 1, ctx->y, 1, ctx->c, ctx->s);
}

static void reset(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    memcpy(ctx->x, ctx->x0, (size_t) ctx->n * BMB_ELEM_BYTES);
    memcpy(ctx->y, ctx->y0, (size_t) ctx->n * BMB_ELEM_BYTES);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x0);
    free(ctx->y0);
    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

/* Four multiplications and two additions an element. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return 6.0 * d1;
}

/* x and y each read and written. */
static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return 4.0 * BMB_ELEM_BYTES * d1;
}

/* --verify: both vectors, element by element, in double. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    const double c = ctx->c, s = ctx->s;
    size_t n = (size_t) ctx->n, i;
    double *x0 = malloc(6 * n * sizeof(double));
    int ok;

    if (x0 == NULL) {
        return -1;
    }
    double *y0 = x0 + n, *got = y0 + n, *want = got + 2 * n;

    bmb_to_double(x0, ctx->x, n);
    bmb_to_double(y0, ctx->y, n);
    if (!bmb_verify_skip()) {
        call(ctx);
    }
    bmb_perturb(&ctx->y[n - 1]);
    bmb_to_double(got, ctx->x, n);
    bmb_to_double(got + n, ctx->y, n);
    for (i = 0; i < n; i++) {
        want[i] = c * x0[i] + s * y0[i];
        want[n + i] = c * y0[i] - s * x0[i];
    }
    /* Each element is two products, |c| and |s| below 1: its scale is
     * |x0| + |y0|, held in x0, which is no longer needed. */
    for (i = 0; i < n; i++) {
        x0[i] = fabs(x0[i]) + fabs(y0[i]);
    }
    ok = bmb_verify_close("x", got, want, x0, n, bmb_tolerance(2), msg, size)
         && bmb_verify_close("y", got + n, want + n, x0, n, bmb_tolerance(2), msg, size);
    free(x0);
    return !ok;
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(rot);
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.level = 1;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;
    bench.reset = reset;
    bench.verify = verify;
    bench.flops = flops;
    bench.bytes = bytes;

    return bmb_benchmark_main(argc, argv, &bench);
}
