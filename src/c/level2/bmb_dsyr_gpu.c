#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_gpu.h"
#include "bmb_log.h"

/* Square routine: only dim1 (= N) is used, swept via --matrix-dim1.
 * A (N x N, upper triangle) is updated in place: A += alpha * x * x^T. */
typedef struct {
    int n;
    double alpha;
    double *d_a;
    double *d_x;
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1;
    double *h_a;
    double *h_x;
    size_t i;

    (void) dim2;
    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;
    ctx->alpha = 1.0e-6;

    h_a = malloc(n * n * sizeof(double));
    h_x = malloc(n * sizeof(double));
    if (h_a == NULL || h_x == NULL) {
        free(h_a);
        free(h_x);
        free(ctx);
        return NULL;
    }
    for (i = 0; i < n * n; i++) {
        h_a[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < n; i++) {
        h_x[i] = (double) (i % 100) * 0.01 - 0.5;
    }

    ctx->d_a = bmb_gpu_malloc(n * n * sizeof(double));
    ctx->d_x = bmb_gpu_malloc(n * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_a, h_a, n * n * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_x, h_x, n * sizeof(double));
    free(h_a);
    free(h_x);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_dsyr(ctx->n, ctx->alpha, ctx->d_x, 1, ctx->d_a, ctx->n);
    bmb_gpu_synchronize();
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_free(ctx->d_a);
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

    bench.routine_name = "dsyr_gpu";
    bench.dim1_label = "Matrix dim (N)";
    bench.dim2_label = NULL;
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;

    status = bmb_benchmark_main(argc, argv, &bench);
    bmb_gpu_shutdown();
    return status;
}
