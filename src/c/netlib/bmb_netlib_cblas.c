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
