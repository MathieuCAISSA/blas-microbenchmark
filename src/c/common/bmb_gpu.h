#ifndef BMB_GPU_H
#define BMB_GPU_H

#include <stddef.h>

/* Backend-agnostic GPU BLAS interface. Exactly one of bmb_gpu_cuda.c
 * (cuBLAS) or bmb_gpu_rocm.c (rocBLAS) is compiled in, selected by
 * configure's --with-gpu-backend. All matrices/vectors are column-major
 * device buffers (cuBLAS/rocBLAS's native layout) with the same
 * M/N/K/lda/ldb/ldc conventions as CBLAS. uplo/trans/diag/side are not
 * exposed here: every wrapper hardcodes the same choices as its CPU
 * counterpart (Upper, NoTrans, NonUnit, Left) to keep the per-routine
 * *_gpu.c files simple. */

typedef enum {
    BMB_GPU_OK = 0,
    /* No compatible device/driver found at runtime. Callers should map
     * this to exit(77), the Automake "SKIP" convention, rather than a
     * hard failure: it just means this machine has no GPU to test with. */
    BMB_GPU_NO_DEVICE,
    BMB_GPU_ERROR
} bmb_gpu_status_t;

bmb_gpu_status_t bmb_gpu_init(void);
void bmb_gpu_shutdown(void);

void *bmb_gpu_malloc(size_t bytes);
void bmb_gpu_free(void *device_ptr);
void bmb_gpu_memcpy_h2d(void *device_dst, const void *host_src, size_t bytes);
void bmb_gpu_memcpy_d2h(void *host_dst, const void *device_src, size_t bytes);

/* Blocks until all previously issued GPU BLAS calls complete. GPU BLAS
 * calls are asynchronous (queued to a stream); call this at the end of
 * the timed portion, or the wall-clock measurement would only capture
 * queue-submission latency, not actual compute time. */
void bmb_gpu_synchronize(void);

/* Level 1 */
void bmb_gpu_dasum(int n, const double *x, int incx, double *result /* host */);
void bmb_gpu_daxpy(int n, double alpha, const double *x, int incx, double *y, int incy);
void bmb_gpu_dcopy(int n, const double *x, int incx, double *y, int incy);
void bmb_gpu_ddot(int n, const double *x, int incx, const double *y, int incy, double *result /* host */);
void bmb_gpu_dnrm2(int n, const double *x, int incx, double *result /* host */);
void bmb_gpu_dscal(int n, double alpha, double *x, int incx);
void bmb_gpu_dswap(int n, double *x, int incx, double *y, int incy);

/* Level 2 */
void bmb_gpu_dgemv(int m, int n, double alpha, const double *a, int lda,
                    const double *x, int incx, double beta, double *y, int incy);
void bmb_gpu_dger(int m, int n, double alpha, const double *x, int incx,
                   const double *y, int incy, double *a, int lda);
void bmb_gpu_dsymv(int n, double alpha, const double *a, int lda,
                    const double *x, int incx, double beta, double *y, int incy);
void bmb_gpu_dsyr(int n, double alpha, const double *x, int incx, double *a, int lda);
void bmb_gpu_dsyr2(int n, double alpha, const double *x, int incx,
                    const double *y, int incy, double *a, int lda);
void bmb_gpu_dtrmv(int n, const double *a, int lda, double *x, int incx);
void bmb_gpu_dtrsv(int n, const double *a, int lda, double *x, int incx);

/* Level 3 */
void bmb_gpu_dgemm(int m, int n, int k, double alpha, const double *a, int lda,
                    const double *b, int ldb, double beta, double *c, int ldc);
void bmb_gpu_dsymm(int m, int n, double alpha, const double *a, int lda,
                    const double *b, int ldb, double beta, double *c, int ldc);
void bmb_gpu_dsyrk(int n, int k, double alpha, const double *a, int lda,
                    double beta, double *c, int ldc);
void bmb_gpu_dsyr2k(int n, int k, double alpha, const double *a, int lda,
                     const double *b, int ldb, double beta, double *c, int ldc);
void bmb_gpu_dtrmm(int m, int n, double alpha, const double *a, int lda, double *b, int ldb);
void bmb_gpu_dtrsm(int m, int n, double alpha, const double *a, int lda, double *b, int ldb);

#endif /* BMB_GPU_H */
