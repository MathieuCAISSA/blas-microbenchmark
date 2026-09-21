#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_gpu.h"
#include "bmb_log.h"

/* dim1 = M, dim2 = N. A (M x N) is updated in place: A += alpha * x * y^T. */
typedef struct {
    int m;
    int n;
    double alpha;
    double *d_a;
    double *d_x;
    double *d_y;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    double *h_a;
    double *h_x;
    double *h_y;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) dim1;
    ctx->n = (int) dim2;
    /* Small alpha: A accumulates rank-1 updates across iterations, kept bounded
     * regardless of --iterations. */
    ctx->alpha = 1.0e-6;

    h_a = malloc(dim1 * dim2 * sizeof(double));
    h_x = malloc(dim1 * sizeof(double));
    h_y = malloc(dim2 * sizeof(double));
    if (h_a == NULL || h_x == NULL || h_y == NULL) {
        free(h_a);
        free(h_x);
        free(h_y);
        free(ctx);
        return NULL;
    }
    for (i = 0; i < dim1 * dim2; i++) {
        h_a[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < dim1; i++) {
        h_x[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < dim2; i++) {
        h_y[i] = (double) ((i + 7) % 100) * 0.01 - 0.5;
    }

    ctx->d_a = bmb_gpu_malloc(dim1 * dim2 * sizeof(double));
    ctx->d_x = bmb_gpu_malloc(dim1 * sizeof(double));
    ctx->d_y = bmb_gpu_malloc(dim2 * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_a, h_a, dim1 * dim2 * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_x, h_x, dim1 * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_y, h_y, dim2 * sizeof(double));
    free(h_a);
    free(h_x);
    free(h_y);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_dger(ctx->m, ctx->n, ctx->alpha, ctx->d_x, 1, ctx->d_y, 1, ctx->d_a, ctx->m);
    bmb_gpu_synchronize();
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_free(ctx->d_a);
    bmb_gpu_free(ctx->d_x);
    bmb_gpu_free(ctx->d_y);
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

    bench.routine_name = "dger_gpu";
    bench.dim1_label = "Matrix dim1 (M)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;

    status = bmb_benchmark_main(argc, argv, &bench);
    bmb_gpu_shutdown();
    return status;
}
