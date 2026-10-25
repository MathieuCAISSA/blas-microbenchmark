#include <complex.h>
#include <stddef.h>

#include "cblas.h"

/* CBLAS entry points implemented over the Fortran BLAS ABI, for netlib
 * reference BLAS (which ships no C interface of its own).
 *
 * Two conventions have to be bridged:
 *
 *  - Fortran passes everything by reference, including scalars, and
 *    gfortran appends a hidden length argument per CHARACTER argument.
 *    Those hidden lengths are declared and passed explicitly here rather
 *    than relying on them being ignorable.
 *
 *  - Fortran is column-major, CBLAS callers may be row-major. A row-major
 *    M x N buffer with leading dimension lda is bit-identical to the
 *    column-major N x M buffer with the same lda -- i.e. its transpose --
 *    so every row-major call becomes a column-major call on the transposed
 *    problem: dimensions swap, and side/uplo/trans flip as the algebra
 *    requires. Column-major callers pass straight through. */

extern double dasum_(const int *n, const double *x, const int *incx);
extern void daxpy_(const int *n, const double *alpha, const double *x, const int *incx,
                   double *y, const int *incy);
extern void dcopy_(const int *n, const double *x, const int *incx, double *y, const int *incy);
extern double ddot_(const int *n, const double *x, const int *incx,
                    const double *y, const int *incy);
extern double dnrm2_(const int *n, const double *x, const int *incx);
extern void dscal_(const int *n, const double *alpha, double *x, const int *incx);
extern void dswap_(const int *n, double *x, const int *incx, double *y, const int *incy);
extern void saxpy_(const int *n, const float *alpha, const float *x, const int *incx,
                   float *y, const int *incy);
/* A float, as gfortran returns a REAL function's value; a library
 * translated by f2c would return a double. */
extern float sdot_(const int *n, const float *x, const int *incx,
                   const float *y, const int *incy);
/* A COMPLEX*16 function: gfortran returns its value as C's double
 * complex. */
extern double complex zdotc_(const int *n, const void *x, const int *incx,
                             const void *y, const int *incy);
/* The other precisions (#226). A REAL function returns a float, as
 * sdot_ does, and a COMPLEX one C's float complex. */
extern float sasum_(const int *n, const float *x, const int *incx);
extern float scasum_(const int *n, const void *x, const int *incx);
extern double dzasum_(const int *n, const void *x, const int *incx);
extern void caxpy_(const int *n, const void *alpha, const void *x, const int *incx,
                   void *y, const int *incy);
extern void zaxpy_(const int *n, const void *alpha, const void *x, const int *incx,
                   void *y, const int *incy);
extern void scopy_(const int *n, const float *x, const int *incx, float *y, const int *incy);
extern void ccopy_(const int *n, const void *x, const int *incx, void *y, const int *incy);
extern void zcopy_(const int *n, const void *x, const int *incx, void *y, const int *incy);
extern float complex cdotc_(const int *n, const void *x, const int *incx,
                            const void *y, const int *incy);
extern float snrm2_(const int *n, const float *x, const int *incx);
extern float scnrm2_(const int *n, const void *x, const int *incx);
extern double dznrm2_(const int *n, const void *x, const int *incx);
extern void sscal_(const int *n, const float *alpha, float *x, const int *incx);
extern void cscal_(const int *n, const void *alpha, void *x, const int *incx);
extern void zscal_(const int *n, const void *alpha, void *x, const int *incx);
extern void sswap_(const int *n, float *x, const int *incx, float *y, const int *incy);
extern void cswap_(const int *n, void *x, const int *incx, void *y, const int *incy);
extern void zswap_(const int *n, void *x, const int *incx, void *y, const int *incy);

extern void dgemv_(const char *trans, const int *m, const int *n, const double *alpha,
                   const double *a, const int *lda, const double *x, const int *incx,
                   const double *beta, double *y, const int *incy, size_t trans_len);
extern void dger_(const int *m, const int *n, const double *alpha,
                  const double *x, const int *incx, const double *y, const int *incy,
                  double *a, const int *lda);
extern void dsymv_(const char *uplo, const int *n, const double *alpha,
                   const double *a, const int *lda, const double *x, const int *incx,
                   const double *beta, double *y, const int *incy, size_t uplo_len);
extern void dsyr_(const char *uplo, const int *n, const double *alpha,
                  const double *x, const int *incx, double *a, const int *lda, size_t uplo_len);
extern void dsyr2_(const char *uplo, const int *n, const double *alpha,
                   const double *x, const int *incx, const double *y, const int *incy,
                   double *a, const int *lda, size_t uplo_len);
extern void dtrmv_(const char *uplo, const char *trans, const char *diag, const int *n,
                   const double *a, const int *lda, double *x, const int *incx,
                   size_t uplo_len, size_t trans_len, size_t diag_len);
extern void dtrsv_(const char *uplo, const char *trans, const char *diag, const int *n,
                   const double *a, const int *lda, double *x, const int *incx,
                   size_t uplo_len, size_t trans_len, size_t diag_len);
extern void sgemv_(const char *trans, const int *m, const int *n, const float *alpha,
                   const float *a, const int *lda, const float *x, const int *incx,
                   const float *beta, float *y, const int *incy, size_t trans_len);
/* The other precisions (#228). */
extern void cgemv_(const char *trans, const int *m, const int *n, const void *alpha,
                   const void *a, const int *lda, const void *x, const int *incx,
                   const void *beta, void *y, const int *incy, size_t trans_len);
extern void zgemv_(const char *trans, const int *m, const int *n, const void *alpha,
                   const void *a, const int *lda, const void *x, const int *incx,
                   const void *beta, void *y, const int *incy, size_t trans_len);
extern void sger_(const int *m, const int *n, const float *alpha,
                  const float *x, const int *incx, const float *y, const int *incy,
                  float *a, const int *lda);
extern void ssymv_(const char *uplo, const int *n, const float *alpha,
                   const float *a, const int *lda, const float *x, const int *incx,
                   const float *beta, float *y, const int *incy, size_t uplo_len);
extern void ssyr_(const char *uplo, const int *n, const float *alpha,
                  const float *x, const int *incx, float *a, const int *lda, size_t uplo_len);
extern void ssyr2_(const char *uplo, const int *n, const float *alpha,
                   const float *x, const int *incx, const float *y, const int *incy,
                   float *a, const int *lda, size_t uplo_len);
extern void strmv_(const char *uplo, const char *trans, const char *diag, const int *n,
                   const float *a, const int *lda, float *x, const int *incx,
                   size_t uplo_len, size_t trans_len, size_t diag_len);
extern void ctrmv_(const char *uplo, const char *trans, const char *diag, const int *n,
                   const void *a, const int *lda, void *x, const int *incx,
                   size_t uplo_len, size_t trans_len, size_t diag_len);
extern void ztrmv_(const char *uplo, const char *trans, const char *diag, const int *n,
                   const void *a, const int *lda, void *x, const int *incx,
                   size_t uplo_len, size_t trans_len, size_t diag_len);
extern void strsv_(const char *uplo, const char *trans, const char *diag, const int *n,
                   const float *a, const int *lda, float *x, const int *incx,
                   size_t uplo_len, size_t trans_len, size_t diag_len);
extern void ctrsv_(const char *uplo, const char *trans, const char *diag, const int *n,
                   const void *a, const int *lda, void *x, const int *incx,
                   size_t uplo_len, size_t trans_len, size_t diag_len);
extern void ztrsv_(const char *uplo, const char *trans, const char *diag, const int *n,
                   const void *a, const int *lda, void *x, const int *incx,
                   size_t uplo_len, size_t trans_len, size_t diag_len);

extern void dgemm_(const char *transa, const char *transb, const int *m, const int *n,
                   const int *k, const double *alpha, const double *a, const int *lda,
                   const double *b, const int *ldb, const double *beta, double *c,
                   const int *ldc, size_t transa_len, size_t transb_len);
extern void dsymm_(const char *side, const char *uplo, const int *m, const int *n,
                   const double *alpha, const double *a, const int *lda,
                   const double *b, const int *ldb, const double *beta, double *c,
                   const int *ldc, size_t side_len, size_t uplo_len);
extern void dsyrk_(const char *uplo, const char *trans, const int *n, const int *k,
                   const double *alpha, const double *a, const int *lda,
                   const double *beta, double *c, const int *ldc,
                   size_t uplo_len, size_t trans_len);
extern void dsyr2k_(const char *uplo, const char *trans, const int *n, const int *k,
                    const double *alpha, const double *a, const int *lda,
                    const double *b, const int *ldb, const double *beta, double *c,
                    const int *ldc, size_t uplo_len, size_t trans_len);
extern void dtrmm_(const char *side, const char *uplo, const char *transa, const char *diag,
                   const int *m, const int *n, const double *alpha, const double *a,
                   const int *lda, double *b, const int *ldb,
                   size_t side_len, size_t uplo_len, size_t transa_len, size_t diag_len);
extern void dtrsm_(const char *side, const char *uplo, const char *transa, const char *diag,
                   const int *m, const int *n, const double *alpha, const double *a,
                   const int *lda, double *b, const int *ldb,
                   size_t side_len, size_t uplo_len, size_t transa_len, size_t diag_len);

extern void sgemm_(const char *transa, const char *transb, const int *m, const int *n,
                   const int *k, const float *alpha, const float *a, const int *lda,
                   const float *b, const int *ldb, const float *beta, float *c,
                   const int *ldc, size_t transa_len, size_t transb_len);
/* Complex arguments are pairs of reals, which is how Fortran stores a
 * COMPLEX or a COMPLEX*16. */
extern void cgemm_(const char *transa, const char *transb, const int *m, const int *n,
                   const int *k, const void *alpha, const void *a, const int *lda,
                   const void *b, const int *ldb, const void *beta, void *c,
                   const int *ldc, size_t transa_len, size_t transb_len);
extern void zgemm_(const char *transa, const char *transb, const int *m, const int *n,
                   const int *k, const void *alpha, const void *a, const int *lda,
                   const void *b, const int *ldb, const void *beta, void *c,
                   const int *ldc, size_t transa_len, size_t transb_len);

extern void strsm_(const char *side, const char *uplo, const char *transa, const char *diag,
                   const int *m, const int *n, const float *alpha, const float *a,
                   const int *lda, float *b, const int *ldb,
                   size_t side_len, size_t uplo_len, size_t transa_len, size_t diag_len);
extern void ztrsm_(const char *side, const char *uplo, const char *transa, const char *diag,
                   const int *m, const int *n, const void *alpha, const void *a,
                   const int *lda, void *b, const int *ldb,
                   size_t side_len, size_t uplo_len, size_t transa_len, size_t diag_len);
extern void ssyrk_(const char *uplo, const char *trans, const int *n, const int *k,
                   const float *alpha, const float *a, const int *lda,
                   const float *beta, float *c, const int *ldc,
                   size_t uplo_len, size_t trans_len);
extern void zherk_(const char *uplo, const char *trans, const int *n, const int *k,
                   const double *alpha, const void *a, const int *lda,
                   const double *beta, void *c, const int *ldc,
                   size_t uplo_len, size_t trans_len);
extern void zhemm_(const char *side, const char *uplo, const int *m, const int *n,
                   const void *alpha, const void *a, const int *lda,
                   const void *b, const int *ldb, const void *beta, void *c,
                   const int *ldc, size_t side_len, size_t uplo_len);
/* The other precisions (#230). */
extern void ssymm_(const char *side, const char *uplo, const int *m, const int *n, const float *alpha,
                   const float *a, const int *lda, const float *b, const int *ldb, const float *beta,
                   float *c, const int *ldc, size_t side_len, size_t uplo_len);
extern void csymm_(const char *side, const char *uplo, const int *m, const int *n, const void *alpha,
                   const void *a, const int *lda, const void *b, const int *ldb, const void *beta, void *c,
                   const int *ldc, size_t side_len, size_t uplo_len);
extern void zsymm_(const char *side, const char *uplo, const int *m, const int *n, const void *alpha,
                   const void *a, const int *lda, const void *b, const int *ldb, const void *beta, void *c,
                   const int *ldc, size_t side_len, size_t uplo_len);
extern void chemm_(const char *side, const char *uplo, const int *m, const int *n, const void *alpha,
                   const void *a, const int *lda, const void *b, const int *ldb, const void *beta, void *c,
                   const int *ldc, size_t side_len, size_t uplo_len);
extern void csyrk_(const char *uplo, const char *trans, const int *n, const int *k, const void *alpha,
                   const void *a, const int *lda, const void *beta, void *c, const int *ldc,
                   size_t uplo_len, size_t trans_len);
extern void zsyrk_(const char *uplo, const char *trans, const int *n, const int *k, const void *alpha,
                   const void *a, const int *lda, const void *beta, void *c, const int *ldc,
                   size_t uplo_len, size_t trans_len);
extern void cherk_(const char *uplo, const char *trans, const int *n, const int *k, const float *alpha,
                   const void *a, const int *lda, const float *beta, void *c, const int *ldc,
                   size_t uplo_len, size_t trans_len);
extern void ssyr2k_(const char *uplo, const char *trans, const int *n, const int *k, const float *alpha,
                    const float *a, const int *lda, const float *b, const int *ldb, const float *beta,
                    float *c, const int *ldc, size_t uplo_len, size_t trans_len);
extern void csyr2k_(const char *uplo, const char *trans, const int *n, const int *k, const void *alpha,
                    const void *a, const int *lda, const void *b, const int *ldb, const void *beta,
                    void *c, const int *ldc, size_t uplo_len, size_t trans_len);
extern void zsyr2k_(const char *uplo, const char *trans, const int *n, const int *k, const void *alpha,
                    const void *a, const int *lda, const void *b, const int *ldb, const void *beta,
                    void *c, const int *ldc, size_t uplo_len, size_t trans_len);
extern void strmm_(const char *side, const char *uplo, const char *transa, const char *diag, const int *m,
                   const int *n, const float *alpha, const float *a, const int *lda, float *b,
                   const int *ldb, size_t side_len, size_t uplo_len, size_t transa_len, size_t diag_len);
extern void ctrmm_(const char *side, const char *uplo, const char *transa, const char *diag, const int *m,
                   const int *n, const void *alpha, const void *a, const int *lda, void *b, const int *ldb,
                   size_t side_len, size_t uplo_len, size_t transa_len, size_t diag_len);
extern void ztrmm_(const char *side, const char *uplo, const char *transa, const char *diag, const int *m,
                   const int *n, const void *alpha, const void *a, const int *lda, void *b, const int *ldb,
                   size_t side_len, size_t uplo_len, size_t transa_len, size_t diag_len);
extern void ctrsm_(const char *side, const char *uplo, const char *transa, const char *diag, const int *m,
                   const int *n, const void *alpha, const void *a, const int *lda, void *b, const int *ldb,
                   size_t side_len, size_t uplo_len, size_t transa_len, size_t diag_len);

static char bmb_trans(const CBLAS_TRANSPOSE t)
{
    return (t == CblasNoTrans) ? 'N' : ((t == CblasTrans) ? 'T' : 'C');
}

static char bmb_trans_flipped(const CBLAS_TRANSPOSE t)
{
    return (t == CblasNoTrans) ? 'T' : 'N';
}

static char bmb_uplo(const CBLAS_UPLO u)
{
    return (u == CblasUpper) ? 'U' : 'L';
}

static char bmb_uplo_flipped(const CBLAS_UPLO u)
{
    return (u == CblasUpper) ? 'L' : 'U';
}

static char bmb_diag(const CBLAS_DIAG d)
{
    return (d == CblasUnit) ? 'U' : 'N';
}

static char bmb_side(const CBLAS_SIDE s)
{
    return (s == CblasLeft) ? 'L' : 'R';
}

static char bmb_side_flipped(const CBLAS_SIDE s)
{
    return (s == CblasLeft) ? 'R' : 'L';
}

/* Negates the imaginary parts of a complex vector of n elements, in
 * place: twice gives it back exactly. */
static void bmb_conj_f(float *x, const int n, const int inc)
{
    const size_t step = 2 * (size_t) (inc < 0 ? -inc : inc);
    int i;

    for (i = 0; i < n; i++) {
        x[(size_t) i * step + 1] = -x[(size_t) i * step + 1];
    }
}

static void bmb_conj_d(double *x, const int n, const int inc)
{
    const size_t step = 2 * (size_t) (inc < 0 ? -inc : inc);
    int i;

    for (i = 0; i < n; i++) {
        x[(size_t) i * step + 1] = -x[(size_t) i * step + 1];
    }
}

/* Level 1: no storage order involved, straight forwarding. */

double cblas_dasum(const int N, const double *X, const int incX)
{
    return dasum_(&N, X, &incX);
}

void cblas_daxpy(const int N, const double alpha, const double *X, const int incX,
                 double *Y, const int incY)
{
    daxpy_(&N, &alpha, X, &incX, Y, &incY);
}

void cblas_dcopy(const int N, const double *X, const int incX, double *Y, const int incY)
{
    dcopy_(&N, X, &incX, Y, &incY);
}

double cblas_ddot(const int N, const double *X, const int incX, const double *Y, const int incY)
{
    return ddot_(&N, X, &incX, Y, &incY);
}

double cblas_dnrm2(const int N, const double *X, const int incX)
{
    return dnrm2_(&N, X, &incX);
}

void cblas_dscal(const int N, const double alpha, double *X, const int incX)
{
    dscal_(&N, &alpha, X, &incX);
}

void cblas_dswap(const int N, double *X, const int incX, double *Y, const int incY)
{
    dswap_(&N, X, &incX, Y, &incY);
}

void cblas_saxpy(const int N, const float alpha, const float *X, const int incX,
                 float *Y, const int incY)
{
    saxpy_(&N, &alpha, X, &incX, Y, &incY);
}

float cblas_sdot(const int N, const float *X, const int incX, const float *Y, const int incY)
{
    return sdot_(&N, X, &incX, Y, &incY);
}

void cblas_zdotc_sub(const int N, const void *X, const int incX, const void *Y, const int incY,
                     void *dotc)
{
    double complex d = zdotc_(&N, X, &incX, Y, &incY);

    ((double *) dotc)[0] = creal(d);
    ((double *) dotc)[1] = cimag(d);
}

float cblas_sasum(const int N, const float *X, const int incX)
{
    return sasum_(&N, X, &incX);
}

float cblas_scasum(const int N, const void *X, const int incX)
{
    return scasum_(&N, X, &incX);
}

double cblas_dzasum(const int N, const void *X, const int incX)
{
    return dzasum_(&N, X, &incX);
}

void cblas_caxpy(const int N, const void *alpha, const void *X, const int incX,
                 void *Y, const int incY)
{
    caxpy_(&N, alpha, X, &incX, Y, &incY);
}

void cblas_zaxpy(const int N, const void *alpha, const void *X, const int incX,
                 void *Y, const int incY)
{
    zaxpy_(&N, alpha, X, &incX, Y, &incY);
}

void cblas_scopy(const int N, const float *X, const int incX, float *Y, const int incY)
{
    scopy_(&N, X, &incX, Y, &incY);
}

void cblas_ccopy(const int N, const void *X, const int incX, void *Y, const int incY)
{
    ccopy_(&N, X, &incX, Y, &incY);
}

void cblas_zcopy(const int N, const void *X, const int incX, void *Y, const int incY)
{
    zcopy_(&N, X, &incX, Y, &incY);
}

void cblas_cdotc_sub(const int N, const void *X, const int incX, const void *Y, const int incY,
                     void *dotc)
{
    float complex d = cdotc_(&N, X, &incX, Y, &incY);

    ((float *) dotc)[0] = crealf(d);
    ((float *) dotc)[1] = cimagf(d);
}

float cblas_snrm2(const int N, const float *X, const int incX)
{
    return snrm2_(&N, X, &incX);
}

float cblas_scnrm2(const int N, const void *X, const int incX)
{
    return scnrm2_(&N, X, &incX);
}

double cblas_dznrm2(const int N, const void *X, const int incX)
{
    return dznrm2_(&N, X, &incX);
}

void cblas_sscal(const int N, const float alpha, float *X, const int incX)
{
    sscal_(&N, &alpha, X, &incX);
}

void cblas_cscal(const int N, const void *alpha, void *X, const int incX)
{
    cscal_(&N, alpha, X, &incX);
}

void cblas_zscal(const int N, const void *alpha, void *X, const int incX)
{
    zscal_(&N, alpha, X, &incX);
}

void cblas_sswap(const int N, float *X, const int incX, float *Y, const int incY)
{
    sswap_(&N, X, &incX, Y, &incY);
}

void cblas_cswap(const int N, void *X, const int incX, void *Y, const int incY)
{
    cswap_(&N, X, &incX, Y, &incY);
}

void cblas_zswap(const int N, void *X, const int incX, void *Y, const int incY)
{
    zswap_(&N, X, &incX, Y, &incY);
}

/* Level 2 */

void cblas_dgemv(const CBLAS_ORDER order, const CBLAS_TRANSPOSE TransA,
                 const int M, const int N, const double alpha, const double *A, const int lda,
                 const double *X, const int incX, const double beta, double *Y, const int incY)
{
    if (order == CblasColMajor) {
        const char trans = bmb_trans(TransA);

        dgemv_(&trans, &M, &N, &alpha, A, &lda, X, &incX, &beta, Y, &incY, 1);
    } else {
        /* The buffer is the N x M transpose in column-major terms, so the
         * transposition request flips with it. */
        const char trans = bmb_trans_flipped(TransA);

        dgemv_(&trans, &N, &M, &alpha, A, &lda, X, &incX, &beta, Y, &incY, 1);
    }
}

void cblas_sgemv(const CBLAS_ORDER order, const CBLAS_TRANSPOSE TransA,
                 const int M, const int N, const float alpha, const float *A, const int lda,
                 const float *X, const int incX, const float beta, float *Y, const int incY)
{
    if (order == CblasColMajor) {
        const char trans = bmb_trans(TransA);

        sgemv_(&trans, &M, &N, &alpha, A, &lda, X, &incX, &beta, Y, &incY, 1);
    } else {
        const char trans = bmb_trans_flipped(TransA);

        sgemv_(&trans, &N, &M, &alpha, A, &lda, X, &incX, &beta, Y, &incY, 1);
    }
}

void cblas_dger(const CBLAS_ORDER order, const int M, const int N, const double alpha,
                const double *X, const int incX, const double *Y, const int incY,
                double *A, const int lda)
{
    if (order == CblasColMajor) {
        dger_(&M, &N, &alpha, X, &incX, Y, &incY, A, &lda);
    } else {
        /* A += alpha*x*y^T row-major is A^T += alpha*y*x^T column-major. */
        dger_(&N, &M, &alpha, Y, &incY, X, &incX, A, &lda);
    }
}

void cblas_dsymv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N,
                 const double alpha, const double *A, const int lda,
                 const double *X, const int incX, const double beta, double *Y, const int incY)
{
    /* A is symmetric, so transposing it only swaps which triangle is stored. */
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);

    dsymv_(&uplo, &N, &alpha, A, &lda, X, &incX, &beta, Y, &incY, 1);
}

void cblas_dsyr(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N, const double alpha,
                const double *X, const int incX, double *A, const int lda)
{
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);

    dsyr_(&uplo, &N, &alpha, X, &incX, A, &lda, 1);
}

void cblas_dsyr2(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N, const double alpha,
                 const double *X, const int incX, const double *Y, const int incY,
                 double *A, const int lda)
{
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);

    dsyr2_(&uplo, &N, &alpha, X, &incX, Y, &incY, A, &lda, 1);
}

void cblas_dtrmv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const double *A, const int lda,
                 double *X, const int incX)
{
    const char diag = bmb_diag(Diag);
    char uplo;
    char trans;

    if (order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(TransA);
    } else {
        /* Transposing swaps the stored triangle and inverts the request. */
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(TransA);
    }

    dtrmv_(&uplo, &trans, &diag, &N, A, &lda, X, &incX, 1, 1, 1);
}

void cblas_dtrsv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const double *A, const int lda,
                 double *X, const int incX)
{
    const char diag = bmb_diag(Diag);
    char uplo;
    char trans;

    if (order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(TransA);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(TransA);
    }

    dtrsv_(&uplo, &trans, &diag, &N, A, &lda, X, &incX, 1, 1, 1);
}

/* The other precisions (#228). A complex matrix asked for row-major
 * with its conjugate transpose is the one case the flip cannot make: the
 * buffer read column-major is B = A^T, and A^H = conj(B), which Fortran
 * has no flag for. conj(A^H x) = B conj(x), so the vectors are
 * conjugated, the call made with 'N' and the result conjugated back, as
 * reference CBLAS does. It conjugates a copy of x; here x is conjugated
 * in place and back, exactly, so as to allocate nothing in a timed call:
 * the benchmarks' x is their own buffer, never read-only memory. */

void cblas_cgemv(const CBLAS_ORDER order, const CBLAS_TRANSPOSE TransA,
                 const int M, const int N, const void *alpha, const void *A, const int lda,
                 const void *X, const int incX, const void *beta, void *Y, const int incY)
{
    if (order == CblasColMajor) {
        const char trans = bmb_trans(TransA);

        cgemv_(&trans, &M, &N, alpha, A, &lda, X, &incX, beta, Y, &incY, 1);
    } else if (TransA != CblasConjTrans) {
        const char trans = bmb_trans_flipped(TransA);

        cgemv_(&trans, &N, &M, alpha, A, &lda, X, &incX, beta, Y, &incY, 1);
    } else {
        /* y = alpha A^H x + beta y: conj(y) = conj(alpha) B conj(x) +
         * conj(beta) conj(y), x of M elements, y of N. */
        const float *al = alpha, *be = beta;
        const float calpha[2] = { al[0], -al[1] }, cbeta[2] = { be[0], -be[1] };
        const char trans = 'N';

        bmb_conj_f((float *) X, M, incX);
        bmb_conj_f(Y, N, incY);
        cgemv_(&trans, &N, &M, calpha, A, &lda, X, &incX, cbeta, Y, &incY, 1);
        bmb_conj_f(Y, N, incY);
        bmb_conj_f((float *) X, M, incX);
    }
}

void cblas_zgemv(const CBLAS_ORDER order, const CBLAS_TRANSPOSE TransA,
                 const int M, const int N, const void *alpha, const void *A, const int lda,
                 const void *X, const int incX, const void *beta, void *Y, const int incY)
{
    if (order == CblasColMajor) {
        const char trans = bmb_trans(TransA);

        zgemv_(&trans, &M, &N, alpha, A, &lda, X, &incX, beta, Y, &incY, 1);
    } else if (TransA != CblasConjTrans) {
        const char trans = bmb_trans_flipped(TransA);

        zgemv_(&trans, &N, &M, alpha, A, &lda, X, &incX, beta, Y, &incY, 1);
    } else {
        const double *al = alpha, *be = beta;
        const double calpha[2] = { al[0], -al[1] }, cbeta[2] = { be[0], -be[1] };
        const char trans = 'N';

        bmb_conj_d((double *) X, M, incX);
        bmb_conj_d(Y, N, incY);
        zgemv_(&trans, &N, &M, calpha, A, &lda, X, &incX, cbeta, Y, &incY, 1);
        bmb_conj_d(Y, N, incY);
        bmb_conj_d((double *) X, M, incX);
    }
}

void cblas_sger(const CBLAS_ORDER order, const int M, const int N, const float alpha,
                const float *X, const int incX, const float *Y, const int incY,
                float *A, const int lda)
{
    if (order == CblasColMajor) {
        sger_(&M, &N, &alpha, X, &incX, Y, &incY, A, &lda);
    } else {
        sger_(&N, &M, &alpha, Y, &incY, X, &incX, A, &lda);
    }
}

void cblas_ssymv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N,
                 const float alpha, const float *A, const int lda,
                 const float *X, const int incX, const float beta, float *Y, const int incY)
{
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);

    ssymv_(&uplo, &N, &alpha, A, &lda, X, &incX, &beta, Y, &incY, 1);
}

void cblas_ssyr(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N, const float alpha,
                const float *X, const int incX, float *A, const int lda)
{
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);

    ssyr_(&uplo, &N, &alpha, X, &incX, A, &lda, 1);
}

void cblas_ssyr2(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const int N, const float alpha,
                 const float *X, const int incX, const float *Y, const int incY,
                 float *A, const int lda)
{
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);

    ssyr2_(&uplo, &N, &alpha, X, &incX, Y, &incY, A, &lda, 1);
}

void cblas_strmv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const float *A, const int lda,
                 float *X, const int incX)
{
    const char diag = bmb_diag(Diag);
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);
    const char trans = (order == CblasColMajor) ? bmb_trans(TransA) : bmb_trans_flipped(TransA);

    strmv_(&uplo, &trans, &diag, &N, A, &lda, X, &incX, 1, 1, 1);
}

/* x = A^H x row-major: conj(x) = B conj(x), as for gemv. */
void cblas_ctrmv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const void *A, const int lda,
                 void *X, const int incX)
{
    const char diag = bmb_diag(Diag);
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);
    const char trans = (order == CblasColMajor) ? bmb_trans(TransA) : bmb_trans_flipped(TransA);
    const int conj = order == CblasRowMajor && TransA == CblasConjTrans;

    if (conj) {
        bmb_conj_f(X, N, incX);
    }
    ctrmv_(&uplo, &trans, &diag, &N, A, &lda, X, &incX, 1, 1, 1);
    if (conj) {
        bmb_conj_f(X, N, incX);
    }
}

void cblas_ztrmv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const void *A, const int lda,
                 void *X, const int incX)
{
    const char diag = bmb_diag(Diag);
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);
    const char trans = (order == CblasColMajor) ? bmb_trans(TransA) : bmb_trans_flipped(TransA);
    const int conj = order == CblasRowMajor && TransA == CblasConjTrans;

    if (conj) {
        bmb_conj_d(X, N, incX);
    }
    ztrmv_(&uplo, &trans, &diag, &N, A, &lda, X, &incX, 1, 1, 1);
    if (conj) {
        bmb_conj_d(X, N, incX);
    }
}

void cblas_strsv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const float *A, const int lda,
                 float *X, const int incX)
{
    const char diag = bmb_diag(Diag);
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);
    const char trans = (order == CblasColMajor) ? bmb_trans(TransA) : bmb_trans_flipped(TransA);

    strsv_(&uplo, &trans, &diag, &N, A, &lda, X, &incX, 1, 1, 1);
}

/* Solving A^H x' = x row-major: B conj(x') = conj(x). */
void cblas_ctrsv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const void *A, const int lda,
                 void *X, const int incX)
{
    const char diag = bmb_diag(Diag);
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);
    const char trans = (order == CblasColMajor) ? bmb_trans(TransA) : bmb_trans_flipped(TransA);
    const int conj = order == CblasRowMajor && TransA == CblasConjTrans;

    if (conj) {
        bmb_conj_f(X, N, incX);
    }
    ctrsv_(&uplo, &trans, &diag, &N, A, &lda, X, &incX, 1, 1, 1);
    if (conj) {
        bmb_conj_f(X, N, incX);
    }
}

void cblas_ztrsv(const CBLAS_ORDER order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE TransA,
                 const CBLAS_DIAG Diag, const int N, const void *A, const int lda,
                 void *X, const int incX)
{
    const char diag = bmb_diag(Diag);
    const char uplo = (order == CblasColMajor) ? bmb_uplo(Uplo) : bmb_uplo_flipped(Uplo);
    const char trans = (order == CblasColMajor) ? bmb_trans(TransA) : bmb_trans_flipped(TransA);
    const int conj = order == CblasRowMajor && TransA == CblasConjTrans;

    if (conj) {
        bmb_conj_d(X, N, incX);
    }
    ztrsv_(&uplo, &trans, &diag, &N, A, &lda, X, &incX, 1, 1, 1);
    if (conj) {
        bmb_conj_d(X, N, incX);
    }
}

/* Level 3 */

void cblas_dgemm(const CBLAS_ORDER Order, const CBLAS_TRANSPOSE TransA, const CBLAS_TRANSPOSE TransB,
                 const int M, const int N, const int K, const double alpha,
                 const double *A, const int lda, const double *B, const int ldb,
                 const double beta, double *C, const int ldc)
{
    const char ta = bmb_trans(TransA);
    const char tb = bmb_trans(TransB);

    if (Order == CblasColMajor) {
        dgemm_(&ta, &tb, &M, &N, &K, &alpha, A, &lda, B, &ldb, &beta, C, &ldc, 1, 1);
    } else {
        /* C^T = op(B)^T * op(A)^T: swap the operands and the two output
         * dimensions, keeping each operand's own transposition flag. */
        dgemm_(&tb, &ta, &N, &M, &K, &alpha, B, &ldb, A, &lda, &beta, C, &ldc, 1, 1);
    }
}

void cblas_sgemm(const CBLAS_ORDER Order, const CBLAS_TRANSPOSE TransA, const CBLAS_TRANSPOSE TransB,
                 const int M, const int N, const int K, const float alpha,
                 const float *A, const int lda, const float *B, const int ldb,
                 const float beta, float *C, const int ldc)
{
    const char ta = bmb_trans(TransA);
    const char tb = bmb_trans(TransB);

    if (Order == CblasColMajor) {
        sgemm_(&ta, &tb, &M, &N, &K, &alpha, A, &lda, B, &ldb, &beta, C, &ldc, 1, 1);
    } else {
        sgemm_(&tb, &ta, &N, &M, &K, &alpha, B, &ldb, A, &lda, &beta, C, &ldc, 1, 1);
    }
}

/* As dgemm. Swapping the operands keeps each one's flag even for a
 * conjugate transpose: (op(A) op(B))^T = op(B)^T op(A)^T, and op(B)^T is
 * B^T, B or conj(B), what the same flag does to the buffer read
 * column-major, B^T. */
void cblas_cgemm(const CBLAS_ORDER Order, const CBLAS_TRANSPOSE TransA, const CBLAS_TRANSPOSE TransB,
                 const int M, const int N, const int K, const void *alpha,
                 const void *A, const int lda, const void *B, const int ldb,
                 const void *beta, void *C, const int ldc)
{
    const char ta = bmb_trans(TransA);
    const char tb = bmb_trans(TransB);

    if (Order == CblasColMajor) {
        cgemm_(&ta, &tb, &M, &N, &K, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    } else {
        cgemm_(&tb, &ta, &N, &M, &K, alpha, B, &ldb, A, &lda, beta, C, &ldc, 1, 1);
    }
}

void cblas_zgemm(const CBLAS_ORDER Order, const CBLAS_TRANSPOSE TransA, const CBLAS_TRANSPOSE TransB,
                 const int M, const int N, const int K, const void *alpha,
                 const void *A, const int lda, const void *B, const int ldb,
                 const void *beta, void *C, const int ldc)
{
    const char ta = bmb_trans(TransA);
    const char tb = bmb_trans(TransB);

    if (Order == CblasColMajor) {
        zgemm_(&ta, &tb, &M, &N, &K, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    } else {
        zgemm_(&tb, &ta, &N, &M, &K, alpha, B, &ldb, A, &lda, beta, C, &ldc, 1, 1);
    }
}

void cblas_dsymm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const int M, const int N, const double alpha,
                 const double *A, const int lda, const double *B, const int ldb,
                 const double beta, double *C, const int ldc)
{
    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        dsymm_(&side, &uplo, &M, &N, &alpha, A, &lda, B, &ldb, &beta, C, &ldc, 1, 1);
    } else {
        /* C^T = B^T * A (A symmetric): the symmetric operand moves to the
         * other side, and its stored triangle flips with the transpose. */
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        dsymm_(&side, &uplo, &N, &M, &alpha, A, &lda, B, &ldb, &beta, C, &ldc, 1, 1);
    }
}

void cblas_dsyrk(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                 const int N, const int K, const double alpha, const double *A, const int lda,
                 const double beta, double *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(Trans);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(Trans);
    }

    dsyrk_(&uplo, &trans, &N, &K, &alpha, A, &lda, &beta, C, &ldc, 1, 1);
}

void cblas_dsyr2k(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                  const int N, const int K, const double alpha,
                  const double *A, const int lda, const double *B, const int ldb,
                  const double beta, double *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(Trans);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(Trans);
    }

    dsyr2k_(&uplo, &trans, &N, &K, &alpha, A, &lda, B, &ldb, &beta, C, &ldc, 1, 1);
}

void cblas_dtrmm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const double alpha, const double *A, const int lda, double *B, const int ldb)
{
    const char transa = bmb_trans(TransA);
    const char diag = bmb_diag(Diag);

    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        dtrmm_(&side, &uplo, &transa, &diag, &M, &N, &alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    } else {
        /* B^T = alpha * B^T * op(A)^T: the triangular operand switches
         * side, and its stored triangle flips; transa is already relative
         * to that transposed operand, so it stays as-is. */
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        dtrmm_(&side, &uplo, &transa, &diag, &N, &M, &alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    }
}

void cblas_dtrsm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const double alpha, const double *A, const int lda, double *B, const int ldb)
{
    const char transa = bmb_trans(TransA);
    const char diag = bmb_diag(Diag);

    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        dtrsm_(&side, &uplo, &transa, &diag, &M, &N, &alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        dtrsm_(&side, &uplo, &transa, &diag, &N, &M, &alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    }
}

void cblas_strsm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const float alpha, const float *A, const int lda, float *B, const int ldb)
{
    const char transa = bmb_trans(TransA);
    const char diag = bmb_diag(Diag);

    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        strsm_(&side, &uplo, &transa, &diag, &M, &N, &alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        strsm_(&side, &uplo, &transa, &diag, &N, &M, &alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    }
}

/* As dtrsm. A conjugate transpose keeps its flag too: the buffer read
 * column-major is A^T, and (A^T)^H is the conj(A) the transposed problem
 * needs. */
void cblas_ztrsm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const void *alpha, const void *A, const int lda, void *B, const int ldb)
{
    const char transa = bmb_trans(TransA);
    const char diag = bmb_diag(Diag);

    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        ztrsm_(&side, &uplo, &transa, &diag, &M, &N, alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        ztrsm_(&side, &uplo, &transa, &diag, &N, &M, alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    }
}

void cblas_ssyrk(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                 const int N, const int K, const float alpha, const float *A, const int lda,
                 const float beta, float *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(Trans);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(Trans);
    }

    ssyrk_(&uplo, &trans, &N, &K, &alpha, A, &lda, &beta, C, &ldc, 1, 1);
}

/* Row-major, the buffer read column-major is C^T = conj(C), Hermitian too
 * and in the other triangle, and A^T: C^T = alpha (A^T)^H A^T + beta C^T,
 * so the triangle flips and NoTrans becomes ConjTrans, and back; zherk
 * takes no plain transpose. */
void cblas_zherk(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                 const int N, const int K, const double alpha, const void *A, const int lda,
                 const double beta, void *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = (Trans == CblasNoTrans) ? 'N' : 'C';
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = (Trans == CblasNoTrans) ? 'C' : 'N';
    }

    zherk_(&uplo, &trans, &N, &K, &alpha, A, &lda, &beta, C, &ldc, 1, 1);
}

/* As dsymm: A^T = conj(A) for a Hermitian A, Hermitian too, stored in the
 * other triangle, so side and triangle flip and nothing is conjugated. */
void cblas_zhemm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const int M, const int N, const void *alpha,
                 const void *A, const int lda, const void *B, const int ldb,
                 const void *beta, void *C, const int ldc)
{
    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        zhemm_(&side, &uplo, &M, &N, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        zhemm_(&side, &uplo, &N, &M, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    }
}

/* The other precisions (#230), as their double or double complex
 * counterpart above: swapping the operands keeps every flag, a
 * conjugate transpose's too, and a complex symmetric matrix flips N and
 * T as a real one does. */

void cblas_ssymm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo, const int M,
                 const int N, const float alpha, const float *A, const int lda, const float *B,
                 const int ldb, const float beta, float *C, const int ldc)
{
    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        ssymm_(&side, &uplo, &M, &N, &alpha, A, &lda, B, &ldb, &beta, C, &ldc, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        ssymm_(&side, &uplo, &N, &M, &alpha, A, &lda, B, &ldb, &beta, C, &ldc, 1, 1);
    }
}

void cblas_csymm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo, const int M,
                 const int N, const void *alpha, const void *A, const int lda, const void *B,
                 const int ldb, const void *beta, void *C, const int ldc)
{
    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        csymm_(&side, &uplo, &M, &N, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        csymm_(&side, &uplo, &N, &M, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    }
}

void cblas_zsymm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo, const int M,
                 const int N, const void *alpha, const void *A, const int lda, const void *B,
                 const int ldb, const void *beta, void *C, const int ldc)
{
    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        zsymm_(&side, &uplo, &M, &N, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        zsymm_(&side, &uplo, &N, &M, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    }
}

void cblas_chemm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo, const int M,
                 const int N, const void *alpha, const void *A, const int lda, const void *B,
                 const int ldb, const void *beta, void *C, const int ldc)
{
    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        chemm_(&side, &uplo, &M, &N, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        chemm_(&side, &uplo, &N, &M, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
    }
}

void cblas_csyrk(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                 const int N, const int K, const void *alpha, const void *A, const int lda,
                 const void *beta, void *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(Trans);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(Trans);
    }

    csyrk_(&uplo, &trans, &N, &K, alpha, A, &lda, beta, C, &ldc, 1, 1);
}

void cblas_zsyrk(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                 const int N, const int K, const void *alpha, const void *A, const int lda,
                 const void *beta, void *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(Trans);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(Trans);
    }

    zsyrk_(&uplo, &trans, &N, &K, alpha, A, &lda, beta, C, &ldc, 1, 1);
}

void cblas_cherk(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                 const int N, const int K, const float alpha, const void *A, const int lda,
                 const float beta, void *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = (Trans == CblasNoTrans) ? 'N' : 'C';
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = (Trans == CblasNoTrans) ? 'C' : 'N';
    }

    cherk_(&uplo, &trans, &N, &K, &alpha, A, &lda, &beta, C, &ldc, 1, 1);
}

void cblas_ssyr2k(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                  const int N, const int K, const float alpha, const float *A, const int lda,
                  const float *B, const int ldb, const float beta, float *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(Trans);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(Trans);
    }

    ssyr2k_(&uplo, &trans, &N, &K, &alpha, A, &lda, B, &ldb, &beta, C, &ldc, 1, 1);
}

void cblas_csyr2k(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                  const int N, const int K, const void *alpha, const void *A, const int lda,
                  const void *B, const int ldb, const void *beta, void *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(Trans);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(Trans);
    }

    csyr2k_(&uplo, &trans, &N, &K, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
}

void cblas_zsyr2k(const CBLAS_ORDER Order, const CBLAS_UPLO Uplo, const CBLAS_TRANSPOSE Trans,
                  const int N, const int K, const void *alpha, const void *A, const int lda,
                  const void *B, const int ldb, const void *beta, void *C, const int ldc)
{
    char uplo;
    char trans;

    if (Order == CblasColMajor) {
        uplo = bmb_uplo(Uplo);
        trans = bmb_trans(Trans);
    } else {
        uplo = bmb_uplo_flipped(Uplo);
        trans = bmb_trans_flipped(Trans);
    }

    zsyr2k_(&uplo, &trans, &N, &K, alpha, A, &lda, B, &ldb, beta, C, &ldc, 1, 1);
}

void cblas_strmm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const float alpha, const float *A, const int lda, float *B, const int ldb)
{
    const char transa = bmb_trans(TransA);
    const char diag = bmb_diag(Diag);

    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        strmm_(&side, &uplo, &transa, &diag, &M, &N, &alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        strmm_(&side, &uplo, &transa, &diag, &N, &M, &alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    }
}

void cblas_ctrmm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const void *alpha, const void *A, const int lda, void *B, const int ldb)
{
    const char transa = bmb_trans(TransA);
    const char diag = bmb_diag(Diag);

    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        ctrmm_(&side, &uplo, &transa, &diag, &M, &N, alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        ctrmm_(&side, &uplo, &transa, &diag, &N, &M, alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    }
}

void cblas_ztrmm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const void *alpha, const void *A, const int lda, void *B, const int ldb)
{
    const char transa = bmb_trans(TransA);
    const char diag = bmb_diag(Diag);

    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        ztrmm_(&side, &uplo, &transa, &diag, &M, &N, alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        ztrmm_(&side, &uplo, &transa, &diag, &N, &M, alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    }
}

void cblas_ctrsm(const CBLAS_ORDER Order, const CBLAS_SIDE Side, const CBLAS_UPLO Uplo,
                 const CBLAS_TRANSPOSE TransA, const CBLAS_DIAG Diag, const int M, const int N,
                 const void *alpha, const void *A, const int lda, void *B, const int ldb)
{
    const char transa = bmb_trans(TransA);
    const char diag = bmb_diag(Diag);

    if (Order == CblasColMajor) {
        const char side = bmb_side(Side);
        const char uplo = bmb_uplo(Uplo);

        ctrsm_(&side, &uplo, &transa, &diag, &M, &N, alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    } else {
        const char side = bmb_side_flipped(Side);
        const char uplo = bmb_uplo_flipped(Uplo);

        ctrsm_(&side, &uplo, &transa, &diag, &N, &M, alpha, A, &lda, B, &ldb, 1, 1, 1, 1);
    }
}
