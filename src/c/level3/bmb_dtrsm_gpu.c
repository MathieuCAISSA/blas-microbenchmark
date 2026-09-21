#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_gpu.h"
#include "bmb_log.h"

/* Side = Left: A is M x M triangular, B and C share the M x N shape.
 * dim1 = M, dim2 = N. B is overwritten in place by the GPU call; it is
 * re-uploaded from the host template before every call so the benchmark
 * is stable across --iterations (mirrors the CPU version's memcpy trick). */
typedef struct {
    int m;
    int n;
    double alpha;
    double *d_a; /* M x M, upper triangle used, diagonally dominant */
    double *d_b;
    double *h_b0; /* template */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2;
    double *h_a;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->alpha = 1.0;

    h_a = malloc(m * m * sizeof(double));
    ctx->h_b0 = malloc(m * n * sizeof(double));
    if (h_a == NULL || ctx->h_b0 == NULL) {
        free(h_a);
        free(ctx->h_b0);
        free(ctx);
        return NULL;
    }
    for (i = 0; i < m; i++) {
        size_t j;

        for (j = 0; j < m; j++) {
            h_a[j * m + i] = (i == j) ? (double) (m + 1) : (0.5 / (double) m);
        }
    }
    for (i = 0; i < m * n; i++) {
        ctx->h_b0[i] = (double) (i % 100) * 0.01 + 1.0;
    }

    ctx->d_a = bmb_gpu_malloc(m * m * sizeof(double));
    ctx->d_b = bmb_gpu_malloc(m * n * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_a, h_a, m * m * sizeof(double));
    free(h_a);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_memcpy_h2d(ctx->d_b, ctx->h_b0, (size_t) ctx->m * (size_t) ctx->n * sizeof(double));
    bmb_gpu_dtrsm(ctx->m, ctx->n, ctx->alpha, ctx->d_a, ctx->m, ctx->d_b, ctx->m);
    bmb_gpu_synchronize();
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_free(ctx->d_a);
    bmb_gpu_free(ctx->d_b);
    free(ctx->h_b0);
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

    bench.routine_name = "dtrsm_gpu";
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
