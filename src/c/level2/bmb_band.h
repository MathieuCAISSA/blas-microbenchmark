/* Band storage (#246), which gbmv and tbsv share: the KL diagonals below
 * the main one and the KU above, each column of the matrix (or row, with
 * --layout row) a column (row) of the band array, of leading dimension
 * KL + KU + 1 at least. A triangular band is KL = 0 (upper) or KU = 0
 * (lower). */
#ifndef BMB_BAND_H
#define BMB_BAND_H

#include <math.h>
#include <stddef.h>

#include "bmb_prec.h"

/* Where element (i, j) of the band is kept, for |i - j| within it:
 * column-major, at row KU + i - j of column j; row-major, the transpose,
 * at column KL + j - i of row i. */
static inline size_t bmb_band_index(size_t i, size_t j, size_t kl, size_t ku, size_t lda, int col)
{
    return col ? (ku + i) - j + j * lda : (kl + j) - i + i * lda;
}

/* y = op(A) x and yabs = |op(A)| |x|, in double, read straight from the
 * band: A M x N, transposed with trans 'T', with a unit diagonal (not
 * read) with unit 1. The band may reach past the matrix's edges (a K of N
 * or more), where nothing is read. As bmb_packed_matvec, without an
 * M x N copy, which --verify, on by default, would pay at any size. */
static inline void bmb_band_matvec(size_t m, size_t n, size_t kl, size_t ku, char trans, int unit,
                                   const bmb_real_t *ab, size_t lda, int col, const double *x,
                                   double *y, double *yabs)
{
    const size_t ny = trans == 'N' ? m : n;
    size_t i, j;

    for (i = 0; i < ny; i++) {
        y[i] = 0.0;
        yabs[i] = 0.0;
    }
    for (i = 0; i < m; i++) {
        const size_t lo = i > kl ? i - kl : 0;
        const size_t hi = ku < n - 1 - i ? i + ku : n - 1;

        for (j = lo; j <= hi && j < n; j++) {
            double a;

            if (unit && i == j) {
                continue;
            }
            a = ab[bmb_band_index(i, j, kl, ku, lda, col)];
            if (trans == 'N') {
                y[i] += a * x[j];
                yabs[i] += fabs(a * x[j]);
            } else {
                y[j] += a * x[i];
                yabs[j] += fabs(a * x[i]);
            }
        }
    }
    if (unit) {
        for (i = 0; i < m && i < n; i++) {
            y[i] += x[i];
            yabs[i] += fabs(x[i]);
        }
    }
}

#endif /* BMB_BAND_H */
