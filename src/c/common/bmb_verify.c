#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "bmb_verify.h"

/* Element (r, c) of A, whichever way it is stored, in whichever precision,
 * real or complex: its real part in *re, its imaginary part in *im. */
static inline void bmb_verify_elem(const void *a, int single, int complex_, int column_major,
                                   size_t lda, size_t r, size_t c, double *re, double *im)
{
    size_t k = column_major ? r + c * lda : r * lda + c;

    if (complex_) {
        k *= 2;
        *im = single ? (double) ((const float *) a)[k + 1] : ((const double *) a)[k + 1];
    } else {
        *im = 0.0;
    }
    *re = single ? (double) ((const float *) a)[k] : ((const double *) a)[k];
}

/* Element (i, j) of the matrix shape describes, op() included, read from
 * A where it is stored: 0 when the shape has none there. */
static int bmb_verify_entry(int shape, int column_major, int complex_, int single,
                            const void *a, size_t lda, size_t i, size_t j,
                            double *ar, double *ai)
{
    const int base = shape & 0xff;
    const int t = (shape & (BMB_VERIFY_TRANS | BMB_VERIFY_CONJ_TRANS)) != 0;
    /* (r, c): the element of the stored triangle op() takes it from. */
    const size_t r = t ? j : i, c = t ? i : j;

    switch (base) {
    case BMB_VERIFY_FULL_T:
        bmb_verify_elem(a, single, complex_, column_major, lda, j, i, ar, ai);
        return 1;
    case BMB_VERIFY_FULL_H:
        bmb_verify_elem(a, single, complex_, column_major, lda, j, i, ar, ai);
        *ai = -*ai;
        return 1;
    case BMB_VERIFY_SYM_UPPER:
        bmb_verify_elem(a, single, complex_, column_major, lda, j >= i ? i : j, j >= i ? j : i, ar, ai);
        return 1;
    case BMB_VERIFY_SYM_LOWER:
        bmb_verify_elem(a, single, complex_, column_major, lda, j >= i ? j : i, j >= i ? i : j, ar, ai);
        return 1;
    case BMB_VERIFY_HERM_UPPER:
        bmb_verify_elem(a, single, complex_, column_major, lda, j >= i ? i : j, j >= i ? j : i, ar, ai);
        *ai = (j > i) ? *ai : (j < i) ? -*ai : 0.0;
        return 1;
    case BMB_VERIFY_HERM_LOWER:
        bmb_verify_elem(a, single, complex_, column_major, lda, j >= i ? j : i, j >= i ? i : j, ar, ai);
        *ai = (i > j) ? *ai : (i < j) ? -*ai : 0.0;
        return 1;
    case BMB_VERIFY_UPPER:
    case BMB_VERIFY_LOWER:
        if (base == BMB_VERIFY_UPPER ? r > c : r < c) {
            return 0;
        }
        if (r == c && (shape & BMB_VERIFY_UNIT)) {
            *ar = 1.0;
            *ai = 0.0;
            return 1;
        }
        bmb_verify_elem(a, single, complex_, column_major, lda, r, c, ar, ai);
        if (shape & BMB_VERIFY_CONJ_TRANS) {
            *ai = -*ai;
        }
        return 1;
    case BMB_VERIFY_LOWER_STRICT:
        if (i <= j) {
            return 0;
        }
        break;
    case BMB_VERIFY_UPPER_STRICT:
        if (i >= j) {
            return 0;
        }
        break;
    default: /* FULL */
        break;
    }
    bmb_verify_elem(a, single, complex_, column_major, lda, i, j, ar, ai);
    return 1;
}

/* bmb_verify_matvec for every precision. Each row sums its terms in the
 * order of j, so that every shape adds what it reads in the order it
 * always did. */
static void bmb_verify_matvec_any(int shape, int column_major, int complex_,
                                  int single, size_t m, size_t n, const void *a, size_t lda,
                                  const double *x, const double *xabs, double *y, double *yabs)
{
    const int base = shape & 0xff;
    const int transposed = base == BMB_VERIFY_FULL_T || base == BMB_VERIFY_FULL_H;
    size_t rows = (base == BMB_VERIFY_FULL) ? m : n;
    size_t cols = transposed ? m : n;
    size_t i, j;

    for (i = 0; i < rows; i++) {
        double re = 0.0, im = 0.0, sa = 0.0;

        for (j = 0; j < cols; j++) {
            double ar, ai;

            if (!bmb_verify_entry(shape, column_major, complex_, single, a, lda, i, j, &ar, &ai)) {
                continue;
            }
            if (!complex_) {
                re += ar * x[j];
                sa += fabs(ar) * (xabs ? xabs[j] : fabs(x[j]));
            } else {
                double xr = x[2 * j], xi = x[2 * j + 1];

                re += ar * xr - ai * xi;
                im += ar * xi + ai * xr;
                sa += (fabs(ar) + fabs(ai)) * (xabs ? xabs[j] : fabs(xr) + fabs(xi));
            }
        }
        if (!complex_) {
            y[i] = re;
        } else {
            y[2 * i] = re;
            y[2 * i + 1] = im;
        }
        yabs[i] = sa;
    }
}

void bmb_verify_matvec(int shape, int column_major, size_t m, size_t n,
                       const double *a, size_t lda,
                       const double *x, const double *xabs,
                       double *y, double *yabs)
{
    bmb_verify_matvec_any(shape, column_major, 0, 0, m, n, a, lda, x, xabs, y, yabs);
}

void bmb_verify_matvec_single(int shape, int column_major, size_t m, size_t n,
                              const float *a, size_t lda,
                              const double *x, const double *xabs,
                              double *y, double *yabs)
{
    bmb_verify_matvec_any(shape, column_major, 0, 1, m, n, a, lda, x, xabs, y, yabs);
}

void bmb_verify_matvec_complex(int shape, int column_major, size_t m, size_t n,
                               const double *a, size_t lda,
                               const double *x, const double *xabs,
                               double *y, double *yabs)
{
    bmb_verify_matvec_any(shape, column_major, 1, 0, m, n, a, lda, x, xabs, y, yabs);
}

void bmb_verify_matvec_complex_single(int shape, int column_major,
                                      size_t m, size_t n, const float *a, size_t lda,
                                      const double *x, const double *xabs,
                                      double *y, double *yabs)
{
    bmb_verify_matvec_any(shape, column_major, 1, 1, m, n, a, lda, x, xabs, y, yabs);
}

double bmb_verify_tolerance(size_t terms)
{
    /* A sum of t products has an error of at most about t * eps times the
     * sum of their magnitudes; the library's own order of summation and
     * the reference's each contribute theirs, and FMA or not changes the
     * constant, hence the factor 8. That is 2.3e-10 for the 2^17 terms of
     * a 65536-wide product, while a wrong result is usually off by a
     * relative 1, and the tests' corruption is 1e-6. */
    return 8.0 * ((double) terms + 4.0) * DBL_EPSILON;
}

double bmb_verify_tolerance_single(size_t terms)
{
    return 8.0 * ((double) terms + 4.0) * FLT_EPSILON;
}

int bmb_verify_close(const char *what, const double *got, const double *want,
                     const double *scale, size_t n, double tol,
                     char *msg, size_t size)
{
    size_t i, worst = 0;
    double worst_excess = 0.0;
    int ok = 1;

    for (i = 0; i < n; i++) {
        double diff = fabs(got[i] - want[i]);
        double allowed = scale ? tol * scale[i] : 0.0;

        /* NaN compares false to anything: !(diff <= allowed) catches it. */
        if (!(diff <= allowed)) {
            double excess = (allowed > 0.0) ? diff / allowed : diff;

            if (ok || !(excess <= worst_excess)) {
                worst = i;
                worst_excess = excess;
            }
            ok = 0;
        }
    }
    if (!ok && n == 1) {
        snprintf(msg, size, "%s is %.17g where %.17g was expected (difference %.3g, allowed %.3g)",
                 what, got[0], want[0], fabs(got[0] - want[0]), scale ? tol * scale[0] : 0.0);
    } else if (!ok) {
        snprintf(msg, size,
                 "element %zu of %s is %.17g where %.17g was expected "
                 "(difference %.3g, allowed %.3g)",
                 worst, what, got[worst], want[worst], fabs(got[worst] - want[worst]),
                 scale ? tol * scale[worst] : 0.0);
    }
    return ok;
}

void bmb_verify_probe(double *x, size_t n)
{
    unsigned long state = 2463534242UL;
    size_t i;

    for (i = 0; i < n; i++) {
        /* xorshift: deterministic, so a failure can be reproduced. */
        state ^= state << 13;
        state &= 0xffffffffUL;
        state ^= state >> 17;
        state ^= state << 5;
        state &= 0xffffffffUL;
        x[i] = (0.5 + (double) (state % 1000) / 1998.0) * ((state & 0x10000UL) ? -1.0 : 1.0);
    }
}

void bmb_verify_perturb(double *value)
{
    const char *env = getenv("BMB_VERIFY_CORRUPT");

    if (env != NULL && env[0] != '\0' && env[0] != '0') {
        *value += 1.0e-6 * (fabs(*value) + 1.0);
    }
}

void bmb_verify_perturb_single(float *value)
{
    const char *env = getenv("BMB_VERIFY_CORRUPT");

    if (env != NULL && env[0] != '\0' && env[0] != '0') {
        *value += fabsf(*value) + 1.0f;
    }
}

int bmb_verify_skip(void)
{
    const char *env = getenv("BMB_VERIFY_SKIP");

    return env != NULL && env[0] != '\0' && env[0] != '0';
}
