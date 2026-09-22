#ifndef BMB_NETLIB_CBLAS_H
#define BMB_NETLIB_CBLAS_H

/* Minimal CBLAS interface for the netlib backend.
 *
 * Netlib reference BLAS ships only the Fortran ABI -- no cblas.h, no
 * cblas_* symbols (Debian/Ubuntu's libblas-dev included) -- so this header
 * plus bmb_netlib_cblas.c provide the CBLAS entry points the benchmarks
 * call, forwarding to the Fortran routines.
 *
 * Only the routines this project benchmarks are declared. The directory
 * holding this file is added to the include path *only* when
 * --with-blas-backend=netlib is selected, so it never shadows the real
 * cblas.h of a backend that has one. */

typedef enum { CblasRowMajor = 101, CblasColMajor = 102 } CBLAS_ORDER;
typedef enum { CblasNoTrans = 111, CblasTrans = 112, CblasConjTrans = 113 } CBLAS_TRANSPOSE;
typedef enum { CblasUpper = 121, CblasLower = 122 } CBLAS_UPLO;
typedef enum { CblasNonUnit = 131, CblasUnit = 132 } CBLAS_DIAG;
typedef enum { CblasLeft = 141, CblasRight = 142 } CBLAS_SIDE;

/* Level 1 */
double cblas_dasum(const int N, const double *X, const int incX);
void cblas_daxpy(const int N, const double alpha, const double *X, const int incX,
                 double *Y, const int incY);
void cblas_dcopy(const int N, const double *X, const int incX, double *Y, const int incY);
double cblas_ddot(const int N, const double *X, const int incX, const double *Y, const int incY);
double cblas_dnrm2(const int N, const double *X, const int incX);
void cblas_dscal(const int N, const double alpha, double *X, const int incX);
void cblas_dswap(const int N, double *X, const int incX, double *Y, const int incY);

/* Level 2 */
void cblas_dgemv(const CBLAS_ORDER order, const CBLAS_TRANSPOSE TransA,
                 const int M, const int N, const double alpha, const double *A, const int lda,
                 const double *X, const int incX, const double beta, double *Y, const int incY);
void cblas_dger(const CBLAS_ORDER order, const int M, const int N, const double alpha,
                const double *X, const int incX, const double *Y, const int incY,
                double *A, const int lda);
void cblas_dsymv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N,
                 const double alpha, const double *A, const int lda,
                 const double *X, const int incX, const double beta, double *Y, const int incY);
void cblas_dsyr(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N, const double alpha,
                const double *X, const int incX, double *A, const int lda);
void cblas_dsyr2(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N, const double alpha,
                 const double *X, const int incX, const double *Y, const int incY,
                 double *A, const int lda);
void cblas_dtrmv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const double *A, const int lda,
                 double *X, const int incX);
void cblas_dtrsv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const double *A, const int lda,
                 double *X, const int incX);

/* Level 3 */
void cblas_dgemm(const CBLAS_ORDER Order, const CBLAS_TRANSPOSE TransA, const CBLAS_TRANSPOSE TransB,
                 const int M, const int N, const int K, const double alpha,
                 const double *A, const int lda, const double *B, const int ldb,
                 const double beta, double *C, const int ldc);
void cblas_dsymm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const int M, const int N, const double alpha,
                 const double *A, const int lda, const double *B, const int ldb,
                 const double beta, double *C, const int ldc);
void cblas_dsyrk(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                 const int N, const int K, const double alpha, const double *A, const int lda,
                 const double beta, double *C, const int ldc);
void cblas_dsyr2k(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                  const int N, const int K, const double alpha,
                  const double *A, const int lda, const double *B, const int ldb,
                  const double beta, double *C, const int ldc);
void cblas_dtrmm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const double alpha, const double *A, const int lda, double *B, const int ldb);
void cblas_dtrsm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const double alpha, const double *A, const int lda, double *B, const int ldb);

#endif /* BMB_NETLIB_CBLAS_H */
