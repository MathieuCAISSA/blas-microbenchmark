#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_gpu.h"
#include "bmb_log.h"

typedef struct {
    int n;
    double alpha;
    double *d_x;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    double *h_x;
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

    h_x = malloc(dim1 * sizeof(double));
    if (h_x == NULL) {
        free(ctx);
        return NULL;
    }
    for (i = 0; i < dim1; i++) {
        h_x[i] = (double) (i % 100) * 0.01 + 1.0;
    }

    ctx->d_x = bmb_gpu_malloc(dim1 * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_x, h_x, dim1 * sizeof(double));
    free(h_x);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_dscal(ctx->n, ctx->alpha, ctx->d_x, 1);
    bmb_gpu_synchronize();
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_free(ctx->d_x);
    free(ctx);
}

int main(int argc, char *argv[])
{
    bmb_benchmark_t bench = {0};
    bmb_gpu_status_t gpu_status;
    int status;

    gpu_status = bmb_gpu_init();
    if (gpu_status == BMB_GPU_NO_DEVICE) {
        bmb_log_warning("No compatible GPU device found; skipping.");
        return 77;
    }
    if (gpu_status != BMB_GPU_OK) {
        bmb_log_error("GPU backend initialization failed.");
        return EXIT_FAILURE;
    }

    bench.routine_name = "dscal_gpu";
    bench.dim1_label = "Vector size";
    bench.dim2_label = NULL;
    bench.use_vector_range = 1;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;

    status = bmb_benchmark_main(argc, argv, &bench);
    bmb_gpu_shutdown();
    return status;
}
