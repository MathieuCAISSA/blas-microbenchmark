#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_verify.h"

/* The conjugated dot product, conj(x)' * y, of two complex vectors. Every
 * complex number is a pair of doubles, real part first, as for zgemm; the
 * result comes back through a pointer (cblas_zdotc_sub), as CBLAS gives
 * it for complex numbers. */
typedef struct {
    int n;
    double *x;
    double *y;
    double dot[2];
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
    ctx->x = bmb_bench_alloc(2 * dim1 * sizeof(double));
    ctx->y = bmb_bench_alloc(2 * dim1 * sizeof(double));
    if (ctx->x == NULL || ctx->y == NULL) {
        free(ctx->x);
        free(ctx->y);
        free(ctx);
        return NULL;
    }

    for (i = 0; i < dim1; i++) {
        ctx->x[2 * i] = (double) (i % 100) * 0.01 - 0.5;
        ctx->x[2 * i + 1] = (double) ((i + 31) % 100) * 0.01 - 0.5;
        ctx->y[2 * i] = (double) ((i + 7) % 100) * 0.01 - 0.5;
        ctx->y[2 * i + 1] = (double) ((i + 53) % 100) * 0.01 - 0.5;
    }

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    cblas_zdotc_sub(ctx->n, (const void *) ctx->x, 1, (const void *) ctx->y, 1, (void *) ctx->dot);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x);
    free(ctx->y);
    free(ctx);
}

/* ddot's count, four times over: a complex multiply-add is 8 real
 * operations. */
static double flops(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;

    return 8.0 * d1;
}

/* Two vectors of 16-byte elements, read once. */
static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;

    return 32.0 * d1;
}

/* --verify: the conjugated dot product, recomputed. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    double got[2], want[2] = {0.0, 0.0}, scale[2];
    double sum = 0.0;
    int i;

    call(ctx);
    got[0] = ctx->dot[0];
    got[1] = ctx->dot[1];
    bmb_verify_perturb(&got[0]);
    for (i = 0; i < ctx->n; i++) {
        double xr = ctx->x[2 * i], xi = ctx->x[2 * i + 1];
        double yr = ctx->y[2 * i], yi = ctx->y[2 * i + 1];

        /* (xr - i xi)(yr + i yi) */
        want[0] += xr * yr + xi * yi;
        want[1] += xr * yi - xi * yr;
        sum += (fabs(xr) + fabs(xi)) * (fabs(yr) + fabs(yi));
    }
    scale[0] = scale[1] = sum;
    return !bmb_verify_close("the dot product (real and imaginary parts in turn)", got, want, scale, 2,
                             bmb_verify_tolerance(2 * (size_t) ctx->n), msg, size);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = "zdotc";
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
