#include <cblas.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_prec.h"
#include "bmb_verify.h"

/* isamax, idamax, icamax and izamax, one per precision (bmb_prec.h): the
 * index of the element of largest absolute value, |re| + |im| for a
 * complex one, the first if several; 0-based, as CBLAS gives it. BLAS
 * puts the i before the precision (BMB_CBLAS_INDEX). */
typedef struct {
    int n;
    size_t max; /* where setup() put the largest element */
    bmb_real_t *x;
    volatile size_t sink;
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

    /* The fill repeats every 100 elements, its largest values tied; one
     * element larger than any other (a real part of 1, where the fill's
     * are at most 0.5), two thirds of the way in, is the answer whatever
     * the library does with ties, and --verify knows it. */
    bmb_fill(ctx->x, dim1, 0, 31);
    ctx->max = 2 * dim1 / 3;
    ctx->x[BMB_PARTS * ctx->max] = BMB_R(1.0);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    ctx->sink = BMB_CBLAS_INDEX(amax)(ctx->n, (const void *) ctx->x, 1);
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    free(ctx->x);
    free(ctx);
}

/* A search: no FLOPs, as copy and swap, and x read once. */
static double bytes(size_t dim1, size_t dim2)
{
    const double d1 = (double) dim1;

    (void) dim2;
    return (double) BMB_ELEM_BYTES * d1;
}

/* --verify: the index setup() put the largest element at, exactly. */
static int verify(void *vctx, char *msg, size_t size)
{
    bmb_ctx_t *ctx = vctx;
    double got, want = (double) ctx->max;

    call(ctx);
    got = (double) ctx->sink;
    bmb_verify_perturb(&got);
    return !bmb_verify_close("the index", &got, &want, NULL, 1, 0.0, msg, size);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};

    bench.routine_name = BMB_ROUTINE_INDEX(amax);
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
