#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_gpu.h"
#include "bmb_log.h"

/* Square routine: only dim1 (= N) is used, swept via --matrix-dim1.
 * X is overwritten in place by the GPU call; it is re-uploaded from the
 * host template before every call so the benchmark is stable across
 * --iterations (mirrors the CPU version's memcpy-before-call trick). */
typedef struct {
    int n;
    double *d_a; /* N x N, column-major, upper triangle used */
    double *d_x;
    double *h_x0; /* template */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t n = dim1;
    double *h_a;
    size_t i;

    (void) dim2;
    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n = (int) n;

    h_a = malloc(n * n * sizeof(double));
    ctx->h_x0 = malloc(n * sizeof(double));
    if (h_a == NULL || ctx->h_x0 == NULL) {
        free(h_a);
        free(ctx->h_x0);
        free(ctx);
        return NULL;
    }
    for (i = 0; i < n; i++) {
        size_t j;

        for (j = 0; j < n; j++) {
            /* Diagonally dominant so the triangular solve is well-conditioned. */
            h_a[j * n + i] = (i == j) ? (double) (n + 1) : (0.5 / (double) n);
        }
        ctx->h_x0[i] = (double) (i % 100) * 0.01 + 1.0;
    }

    ctx->d_a = bmb_gpu_malloc(n * n * sizeof(double));
    ctx->d_x = bmb_gpu_malloc(n * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_a, h_a, n * n * sizeof(double));
    free(h_a);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_memcpy_h2d(ctx->d_x, ctx->h_x0, (size_t) ctx->n * sizeof(double));
    bmb_gpu_dtrsv(ctx->n, ctx->d_a, ctx->n, ctx->d_x, 1);
    bmb_gpu_synchronize();
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_free(ctx->d_a);
    bmb_gpu_free(ctx->d_x);
    free(ctx->h_x0);
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

    bench.routine_name = "dtrsv_gpu";
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
