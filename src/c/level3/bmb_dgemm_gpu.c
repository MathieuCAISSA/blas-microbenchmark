#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_gpu.h"
#include "bmb_log.h"

/* dim1 = M = K, dim2 = N (K is tied to M so the two --matrix-dim options
 * remain sufficient to describe the three GEMM dimensions; see README.md). */
typedef struct {
    int m;
    int n;
    int k;
    double alpha;
    double beta;
    double *d_a; /* M x K */
    double *d_b; /* K x N */
    double *d_c; /* M x N */
} bmb_ctx_t;

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    bmb_ctx_t *ctx;
    size_t m = dim1, n = dim2, k = dim1;
    double *h_a;
    double *h_b;
    double *h_c;
    size_t i;

    (void) thread_count;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->m = (int) m;
    ctx->n = (int) n;
    ctx->k = (int) k;
    ctx->alpha = 1.0;
    ctx->beta = 0.0;

    h_a = malloc(m * k * sizeof(double));
    h_b = malloc(k * n * sizeof(double));
    h_c = malloc(m * n * sizeof(double));
    if (h_a == NULL || h_b == NULL || h_c == NULL) {
        free(h_a);
        free(h_b);
        free(h_c);
        free(ctx);
        return NULL;
    }
    for (i = 0; i < m * k; i++) {
        h_a[i] = (double) (i % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < k * n; i++) {
        h_b[i] = (double) ((i + 7) % 100) * 0.01 - 0.5;
    }
    for (i = 0; i < m * n; i++) {
        h_c[i] = 0.0;
    }

    ctx->d_a = bmb_gpu_malloc(m * k * sizeof(double));
    ctx->d_b = bmb_gpu_malloc(k * n * sizeof(double));
    ctx->d_c = bmb_gpu_malloc(m * n * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_a, h_a, m * k * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_b, h_b, k * n * sizeof(double));
    bmb_gpu_memcpy_h2d(ctx->d_c, h_c, m * n * sizeof(double));
    free(h_a);
    free(h_b);
    free(h_c);

    return ctx;
}

static void call(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_dgemm(ctx->m, ctx->n, ctx->k, ctx->alpha, ctx->d_a, ctx->m, ctx->d_b, ctx->k,
                  ctx->beta, ctx->d_c, ctx->m);
    bmb_gpu_synchronize();
}

static void teardown(void *vctx)
{
    bmb_ctx_t *ctx = vctx;

    bmb_gpu_free(ctx->d_a);
    bmb_gpu_free(ctx->d_b);
    bmb_gpu_free(ctx->d_c);
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

    bench.routine_name = "dgemm_gpu";
    bench.dim1_label = "Matrix dim1 (M=K)";
    bench.dim2_label = "Matrix dim2 (N)";
    bench.use_vector_range = 0;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;

    status = bmb_benchmark_main(argc, argv, &bench);
    bmb_gpu_shutdown();
    return status;
}
