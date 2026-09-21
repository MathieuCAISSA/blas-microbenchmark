#include <cublas_v2.h>
#include <cuda_runtime.h>
#include <stdlib.h>

#include "bmb_gpu.h"
#include "bmb_log.h"

static cublasHandle_t bmb_handle;

static void bmb_cuda_check(cudaError_t status, const char *what)
{
    if (status != cudaSuccess) {
        bmb_log_error(what);
        exit(EXIT_FAILURE);
    }
}

static void bmb_cublas_check(cublasStatus_t status, const char *what)
{
    if (status != CUBLAS_STATUS_SUCCESS) {
        bmb_log_error(what);
        exit(EXIT_FAILURE);
    }
}

bmb_gpu_status_t bmb_gpu_init(void)
{
    int device_count = 0;
    cudaError_t cuda_status = cudaGetDeviceCount(&device_count);

    if (cuda_status != cudaSuccess || device_count == 0) {
        return BMB_GPU_NO_DEVICE;
    }

    bmb_cuda_check(cudaSetDevice(0), "cudaSetDevice(0) failed.");

    if (cublasCreate(&bmb_handle) != CUBLAS_STATUS_SUCCESS) {
        return BMB_GPU_ERROR;
    }

    return BMB_GPU_OK;
}

void bmb_gpu_shutdown(void)
{
    cublasDestroy(bmb_handle);
}

void *bmb_gpu_malloc(size_t bytes)
{
    void *ptr = NULL;

    bmb_cuda_check(cudaMalloc(&ptr, bytes), "cudaMalloc failed.");
    return ptr;
}

void bmb_gpu_free(void *device_ptr)
{
    cudaFree(device_ptr);
}

void bmb_gpu_memcpy_h2d(void *device_dst, const void *host_src, size_t bytes)
{
    bmb_cuda_check(cudaMemcpy(device_dst, host_src, bytes, cudaMemcpyHostToDevice),
                   "cudaMemcpy (host to device) failed.");
}

void bmb_gpu_memcpy_d2h(void *host_dst, const void *device_src, size_t bytes)
{
    bmb_cuda_check(cudaMemcpy(host_dst, device_src, bytes, cudaMemcpyDeviceToHost),
                   "cudaMemcpy (device to host) failed.");
}

void bmb_gpu_synchronize(void)
{
    bmb_cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize failed.");
}

/* Level 1 */

void bmb_gpu_dasum(int n, const double *x, int incx, double *result)
{
    bmb_cublas_check(cublasDasum(bmb_handle, n, x, incx, result), "cublasDasum failed.");
}

void bmb_gpu_daxpy(int n, double alpha, const double *x, int incx, double *y, int incy)
{
    bmb_cublas_check(cublasDaxpy(bmb_handle, n, &alpha, x, incx, y, incy), "cublasDaxpy failed.");
}

void bmb_gpu_dcopy(int n, const double *x, int incx, double *y, int incy)
{
    bmb_cublas_check(cublasDcopy(bmb_handle, n, x, incx, y, incy), "cublasDcopy failed.");
}

void bmb_gpu_ddot(int n, const double *x, int incx, const double *y, int incy, double *result)
{
    bmb_cublas_check(cublasDdot(bmb_handle, n, x, incx, y, incy, result), "cublasDdot failed.");
}

void bmb_gpu_dnrm2(int n, const double *x, int incx, double *result)
{
    bmb_cublas_check(cublasDnrm2(bmb_handle, n, x, incx, result), "cublasDnrm2 failed.");
}

void bmb_gpu_dscal(int n, double alpha, double *x, int incx)
{
    bmb_cublas_check(cublasDscal(bmb_handle, n, &alpha, x, incx), "cublasDscal failed.");
}

void bmb_gpu_dswap(int n, double *x, int incx, double *y, int incy)
{
    bmb_cublas_check(cublasDswap(bmb_handle, n, x, incx, y, incy), "cublasDswap failed.");
}

/* Level 2 */

void bmb_gpu_dgemv(int m, int n, double alpha, const double *a, int lda,
                    const double *x, int incx, double beta, double *y, int incy)
{
    bmb_cublas_check(cublasDgemv(bmb_handle, CUBLAS_OP_N, m, n, &alpha, a, lda, x, incx, &beta, y, incy),
                      "cublasDgemv failed.");
}

void bmb_gpu_dger(int m, int n, double alpha, const double *x, int incx,
                   const double *y, int incy, double *a, int lda)
{
    bmb_cublas_check(cublasDger(bmb_handle, m, n, &alpha, x, incx, y, incy, a, lda), "cublasDger failed.");
}

void bmb_gpu_dsymv(int n, double alpha, const double *a, int lda,
                    const double *x, int incx, double beta, double *y, int incy)
{
    bmb_cublas_check(cublasDsymv(bmb_handle, CUBLAS_FILL_MODE_UPPER, n, &alpha, a, lda, x, incx, &beta, y, incy),
                      "cublasDsymv failed.");
}

void bmb_gpu_dsyr(int n, double alpha, const double *x, int incx, double *a, int lda)
{
    bmb_cublas_check(cublasDsyr(bmb_handle, CUBLAS_FILL_MODE_UPPER, n, &alpha, x, incx, a, lda), "cublasDsyr failed.");
}

void bmb_gpu_dsyr2(int n, double alpha, const double *x, int incx,
                    const double *y, int incy, double *a, int lda)
{
    bmb_cublas_check(cublasDsyr2(bmb_handle, CUBLAS_FILL_MODE_UPPER, n, &alpha, x, incx, y, incy, a, lda),
                      "cublasDsyr2 failed.");
}

void bmb_gpu_dtrmv(int n, const double *a, int lda, double *x, int incx)
{
    bmb_cublas_check(cublasDtrmv(bmb_handle, CUBLAS_FILL_MODE_UPPER, CUBLAS_OP_N, CUBLAS_DIAG_NON_UNIT,
                                   n, a, lda, x, incx),
                      "cublasDtrmv failed.");
}

void bmb_gpu_dtrsv(int n, const double *a, int lda, double *x, int incx)
{
    bmb_cublas_check(cublasDtrsv(bmb_handle, CUBLAS_FILL_MODE_UPPER, CUBLAS_OP_N, CUBLAS_DIAG_NON_UNIT,
                                   n, a, lda, x, incx),
                      "cublasDtrsv failed.");
}

/* Level 3 */

void bmb_gpu_dgemm(int m, int n, int k, double alpha, const double *a, int lda,
                    const double *b, int ldb, double beta, double *c, int ldc)
{
    bmb_cublas_check(cublasDgemm(bmb_handle, CUBLAS_OP_N, CUBLAS_OP_N, m, n, k,
                                   &alpha, a, lda, b, ldb, &beta, c, ldc),
                      "cublasDgemm failed.");
}

void bmb_gpu_dsymm(int m, int n, double alpha, const double *a, int lda,
                    const double *b, int ldb, double beta, double *c, int ldc)
{
    bmb_cublas_check(cublasDsymm(bmb_handle, CUBLAS_SIDE_LEFT, CUBLAS_FILL_MODE_UPPER, m, n,
                                   &alpha, a, lda, b, ldb, &beta, c, ldc),
                      "cublasDsymm failed.");
}

void bmb_gpu_dsyrk(int n, int k, double alpha, const double *a, int lda, double beta, double *c, int ldc)
{
    bmb_cublas_check(cublasDsyrk(bmb_handle, CUBLAS_FILL_MODE_UPPER, CUBLAS_OP_N, n, k,
                                   &alpha, a, lda, &beta, c, ldc),
                      "cublasDsyrk failed.");
}

void bmb_gpu_dsyr2k(int n, int k, double alpha, const double *a, int lda,
                     const double *b, int ldb, double beta, double *c, int ldc)
{
    bmb_cublas_check(cublasDsyr2k(bmb_handle, CUBLAS_FILL_MODE_UPPER, CUBLAS_OP_N, n, k,
                                    &alpha, a, lda, b, ldb, &beta, c, ldc),
                      "cublasDsyr2k failed.");
}

void bmb_gpu_dtrmm(int m, int n, double alpha, const double *a, int lda, double *b, int ldb)
{
    /* Unlike CBLAS's in-place dtrmm, cuBLAS writes to a separate output
     * buffer; passing b/ldb as both input and output makes it behave
     * in-place, matching the CPU routine's semantics. */
    bmb_cublas_check(cublasDtrmm(bmb_handle, CUBLAS_SIDE_LEFT, CUBLAS_FILL_MODE_UPPER, CUBLAS_OP_N,
                                   CUBLAS_DIAG_NON_UNIT, m, n, &alpha, a, lda, b, ldb, b, ldb),
                      "cublasDtrmm failed.");
}

void bmb_gpu_dtrsm(int m, int n, double alpha, const double *a, int lda, double *b, int ldb)
{
    bmb_cublas_check(cublasDtrsm(bmb_handle, CUBLAS_SIDE_LEFT, CUBLAS_FILL_MODE_UPPER, CUBLAS_OP_N,
                                   CUBLAS_DIAG_NON_UNIT, m, n, &alpha, a, lda, b, ldb),
                      "cublasDtrsm failed.");
}
