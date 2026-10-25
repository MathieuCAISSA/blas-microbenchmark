/* The source of dotc and of dotu, which bmb_dotc.c and bmb_dotu.c
 * compile, the second with BMB_UNCONJ defined (#236): a header, so that
 * the two programs share it without one source including another. */
#ifndef BMB_DOTC_H
#define BMB_DOTC_H

#include <cblas.h>
#include <stdlib.h>
#include <math.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* cdotc and zdotc, one per precision (bmb_prec.h): the conjugated dot
 * product, conj(x)' * y, of two complex vectors; with BMB_UNCONJ defined,
 * cdotu and zdotu, x' * y. The result comes back through a pointer
 * (cblas_?dotc_sub, cblas_?dotu_sub), as CBLAS gives it for complex
 * numbers. */
#if !BMB_COMPLEX
#error "dotc and dotu are complex: the real one is dot"
#endif

#ifdef BMB_UNCONJ
#define BMB_OP dotu
#define BMB_OP_SUB dotu_sub
/* x's imaginary part as the product takes it */
#define BMB_XI(xi) (xi)
#else
#define BMB_OP dotc
#define BMB_OP_SUB dotc_sub
#define BMB_XI(xi) (-(xi))
#endif

typedef struct {
    int n;
    bmb_real_t *x;
    bmb_real_t *y;
    bmb_real_t dot[2];
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

    BMB_CBLAS(BMB_OP_SUB)(ctx->n, (const void *) ctx->x, 1, (const void *) ctx->y, 1, (void *) ctx->dot);
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
    bmb_real_t result[2];
    double got[2], want[2] = {0.0, 0.0}, scale[2];
    double sum = 0.0;
    int i;

    call(ctx);
    result[0] = ctx->dot[0];
    result[1] = ctx->dot[1];
    bmb_perturb(&result[0]);
    got[0] = result[0];
    got[1] = result[1];
    for (i = 0; i < ctx->n; i++) {
        const double xr = ctx->x[2 * i], xi = BMB_XI(ctx->x[2 * i + 1]);
        const double yr = ctx->y[2 * i], yi = ctx->y[2 * i + 1];

        /* (xr + i xi)(yr + i yi), xi negated for dotc */
        want[0] += xr * yr - xi * yi;
        want[1] += xr * yi + xi * yr;
        sum += (fabs(xr) + fabs(xi)) * (fabs(yr) + fabs(yi));
    }
    scale[0] = scale[1] = sum;
    return !bmb_verify_close("the dot product" BMB_PARTS_NOTE, got, want, scale, 2,
                             bmb_tolerance((size_t) ctx->n), msg, size);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE(BMB_OP);
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

#endif /* BMB_DOTC_H */
