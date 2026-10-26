/* Packed storage (#244), which spmv and tpsv share: one triangle of an
 * N x N matrix, N (N + 1) / 2 elements, with no leading dimension. */
#ifndef BMB_PACKED_H
#define BMB_PACKED_H

#include <math.h>
#include <stddef.h>

#include "bmb_prec.h"

/* Where element (i, j) of the triangle --uplo gives is kept: column after
 * column, or row after row with --layout row. Row-major Upper is
 * column-major Lower of the transpose, as for a full matrix: either way,
 * a line (a column, or a row) of the triangle is kept whole, and only
 * whether the lines grow or shrink differs. */
static inline size_t bmb_packed_index(size_t i, size_t j, size_t n, int col, char uplo)
{
    const size_t line = col ? j : i, at = col ? i : j;

    if ((uplo == 'U') == (col != 0)) {
        /* Lines of 1, 2, ... elements: column-major Upper, row-major Lower. */
        return line * (line + 1) / 2 + at;
    }
    /* Lines of n, n - 1, ... elements, each starting on the diagonal. */
    return line * n - line * (line - 1) / 2 + (at - line);
}

/* y = op(A) x and yabs = |op(A)| |x|, in double, read straight from the
 * packed matrix: A symmetric (sym 1), both triangles from the one kept,
 * or triangular, transposed with trans 'T' and with a unit diagonal with
 * diag 'U'. The same products as bmb_verify_matvec's shapes, without an
 * N x N copy: --verify is on by default, and a dense matrix of doubles
 * would take twice the packed one's memory, or four times a float's. */
static inline void bmb_packed_matvec(int sym, char uplo, char trans, char diag, const bmb_real_t *ap,
                                     size_t n, int col, const double *x, double *y, double *yabs)
{
    size_t i, j;

    for (i = 0; i < n; i++) {
        y[i] = 0.0;
        yabs[i] = 0.0;
    }
    for (i = 0; i < n; i++) {
        const size_t lo = uplo == 'U' ? i : 0, hi = uplo == 'U' ? n : i + 1;

        for (j = lo; j < hi; j++) {
            const double a = ap[bmb_packed_index(i, j, n, col, uplo)];

            if (i == j && !sym && diag == 'U') {
                continue;
            }
            if (sym || trans == 'N') {
                y[i] += a * x[j];
                yabs[i] += fabs(a * x[j]);
            }
            if ((sym && i != j) || (!sym && trans != 'N')) {
                y[j] += a * x[i];
                yabs[j] += fabs(a * x[i]);
            }
        }
    }
    if (!sym && diag == 'U') {
        for (i = 0; i < n; i++) {
            y[i] += x[i];
            yabs[i] += fabs(x[i]);
        }
    }
}

#endif /* BMB_PACKED_H */
