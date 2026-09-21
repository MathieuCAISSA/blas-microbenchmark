#include <hip/hip_runtime.h>
#include <rocblas/rocblas.h>
#include <stdlib.h>

#include "bmb_gpu.h"
#include "bmb_log.h"

static rocblas_handle bmb_handle;

static void bmb_hip_check(hipError_t status, const char *what)
{
    if (status != hipSuccess) {
        bmb_log_error(what);
        exit(EXIT_FAILURE);
    }
}

static void bmb_rocblas_check(rocblas_status status, const char *what)
{
    if (status != rocblas_status_success) {
        bmb_log_error(what);
        exit(EXIT_FAILURE);
    }
}

bmb_gpu_status_t bmb_gpu_init(void)
{
    int device_count = 0;
    hipError_t hip_status = hipGetDeviceCount(&device_count);

    if (hip_status != hipSuccess || device_count == 0) {
        return BMB_GPU_NO_DEVICE;
    }

    bmb_hip_check(hipSetDevice(0), "hipSetDevice(0) failed.");

    if (rocblas_create_handle(&bmb_handle) != rocblas_status_success) {
        return BMB_GPU_ERROR;
    }

    return BMB_GPU_OK;
}

void bmb_gpu_shutdown(void)
{
    rocblas_destroy_handle(bmb_handle);
}

void *bmb_gpu_malloc(size_t bytes)
{
    void *ptr = NULL;

    bmb_hip_check(hipMalloc(&ptr, bytes), "hipMalloc failed.");
    return ptr;
}

void bmb_gpu_free(void *device_ptr)
{
    hipFree(device_ptr);
}

void bmb_gpu_memcpy_h2d(void *device_dst, const void *host_src, size_t bytes)
{
    bmb_hip_check(hipMemcpy(device_dst, host_src, bytes, hipMemcpyHostToDevice),
                  "hipMemcpy (host to device) failed.");
}

void bmb_gpu_memcpy_d2h(void *host_dst, const void *device_src, size_t bytes)
{
    bmb_hip_check(hipMemcpy(host_dst, device_src, bytes, hipMemcpyDeviceToHost),
                  "hipMemcpy (device to host) failed.");
}

void bmb_gpu_synchronize(void)
{
    bmb_hip_check(hipDeviceSynchronize(), "hipDeviceSynchronize failed.");
}

/* Level 1 */

void bmb_gpu_dasum(int n, const double *x, int incx, double *result)
{
    bmb_rocblas_check(rocblas_dasum(bmb_handle, n, x, incx, result), "rocblas_dasum failed.");
}

void bmb_gpu_daxpy(int n, double alpha, const double *x, int incx, double *y, int incy)
{
    bmb_rocblas_check(rocblas_daxpy(bmb_handle, n, &alpha, x, incx, y, incy), "rocblas_daxpy failed.");
}

void bmb_gpu_dcopy(int n, const double *x, int incx, double *y, int incy)
{
    bmb_rocblas_check(rocblas_dcopy(bmb_handle, n, x, incx, y, incy), "rocblas_dcopy failed.");
}

void bmb_gpu_ddot(int n, const double *x, int incx, const double *y, int incy, double *result)
{
    bmb_rocblas_check(rocblas_ddot(bmb_handle, n, x, incx, y, incy, result), "rocblas_ddot failed.");
}

void bmb_gpu_dnrm2(int n, const double *x, int incx, double *result)
{
    bmb_rocblas_check(rocblas_dnrm2(bmb_handle, n, x, incx, result), "rocblas_dnrm2 failed.");
}

void bmb_gpu_dscal(int n, double alpha, double *x, int incx)
{
    bmb_rocblas_check(rocblas_dscal(bmb_handle, n, &alpha, x, incx), "rocblas_dscal failed.");
}

void bmb_gpu_dswap(int n, double *x, int incx, double *y, int incy)
{
    bmb_rocblas_check(rocblas_dswap(bmb_handle, n, x, incx, y, incy), "rocblas_dswap failed.");
}

/* Level 2 */

void bmb_gpu_dgemv(int m, int n, double alpha, const double *a, int lda,
                    const double *x, int incx, double beta, double *y, int incy)
{
    bmb_rocblas_check(rocblas_dgemv(bmb_handle, rocblas_operation_none, m, n, &alpha, a, lda, x, incx, &beta, y, incy),
                       "rocblas_dgemv failed.");
}

void bmb_gpu_dger(int m, int n, double alpha, const double *x, int incx,
                   const double *y, int incy, double *a, int lda)
{
    bmb_rocblas_check(rocblas_dger(bmb_handle, m, n, &alpha, x, incx, y, incy, a, lda), "rocblas_dger failed.");
}

void bmb_gpu_dsymv(int n, double alpha, const double *a, int lda,
                    const double *x, int incx, double beta, double *y, int incy)
{
    bmb_rocblas_check(rocblas_dsymv(bmb_handle, rocblas_fill_upper, n, &alpha, a, lda, x, incx, &beta, y, incy),
                       "rocblas_dsymv failed.");
}

void bmb_gpu_dsyr(int n, double alpha, const double *x, int incx, double *a, int lda)
{
    bmb_rocblas_check(rocblas_dsyr(bmb_handle, rocblas_fill_upper, n, &alpha, x, incx, a, lda), "rocblas_dsyr failed.");
}

void bmb_gpu_dsyr2(int n, double alpha, const double *x, int incx,
                    const double *y, int incy, double *a, int lda)
{
    bmb_rocblas_check(rocblas_dsyr2(bmb_handle, rocblas_fill_upper, n, &alpha, x, incx, y, incy, a, lda),
                       "rocblas_dsyr2 failed.");
}

void bmb_gpu_dtrmv(int n, const double *a, int lda, double *x, int incx)
{
    bmb_rocblas_check(rocblas_dtrmv(bmb_handle, rocblas_fill_upper, rocblas_operation_none,
                                     rocblas_diagonal_non_unit, n, a, lda, x, incx),
                       "rocblas_dtrmv failed.");
}

void bmb_gpu_dtrsv(int n, const double *a, int lda, double *x, int incx)
{
    bmb_rocblas_check(rocblas_dtrsv(bmb_handle, rocblas_fill_upper, rocblas_operation_none,
                                     rocblas_diagonal_non_unit, n, a, lda, x, incx),
                       "rocblas_dtrsv failed.");
}

/* Level 3 */

void bmb_gpu_dgemm(int m, int n, int k, double alpha, const double *a, int lda,
                    const double *b, int ldb, double beta, double *c, int ldc)
{
    bmb_rocblas_check(rocblas_dgemm(bmb_handle, rocblas_operation_none, rocblas_operation_none, m, n, k,
                                     &alpha, a, lda, b, ldb, &beta, c, ldc),
                       "rocblas_dgemm failed.");
}

void bmb_gpu_dsymm(int m, int n, double alpha, const double *a, int lda,
                    const double *b, int ldb, double beta, double *c, int ldc)
{
    bmb_rocblas_check(rocblas_dsymm(bmb_handle, rocblas_side_left, rocblas_fill_upper, m, n,
                                     &alpha, a, lda, b, ldb, &beta, c, ldc),
                       "rocblas_dsymm failed.");
}

void bmb_gpu_dsyrk(int n, int k, double alpha, const double *a, int lda, double beta, double *c, int ldc)
{
    bmb_rocblas_check(rocblas_dsyrk(bmb_handle, rocblas_fill_upper, rocblas_operation_none, n, k,
                                     &alpha, a, lda, &beta, c, ldc),
                       "rocblas_dsyrk failed.");
}

void bmb_gpu_dsyr2k(int n, int k, double alpha, const double *a, int lda,
                     const double *b, int ldb, double beta, double *c, int ldc)
{
    bmb_rocblas_check(rocblas_dsyr2k(bmb_handle, rocblas_fill_upper, rocblas_operation_none, n, k,
                                      &alpha, a, lda, b, ldb, &beta, c, ldc),
                       "rocblas_dsyr2k failed.");
}

void bmb_gpu_dtrmm(int m, int n, double alpha, const double *a, int lda, double *b, int ldb)
{
    bmb_rocblas_check(rocblas_dtrmm(bmb_handle, rocblas_side_left, rocblas_fill_upper, rocblas_operation_none,
                                     rocblas_diagonal_non_unit, m, n, &alpha, a, lda, b, ldb),
                       "rocblas_dtrmm failed.");
}

void bmb_gpu_dtrsm(int m, int n, double alpha, const double *a, int lda, double *b, int ldb)
{
    bmb_rocblas_check(rocblas_dtrsm(bmb_handle, rocblas_side_left, rocblas_fill_upper, rocblas_operation_none,
                                     rocblas_diagonal_non_unit, m, n, &alpha, a, lda, b, ldb),
                       "rocblas_dtrsm failed.");
}
