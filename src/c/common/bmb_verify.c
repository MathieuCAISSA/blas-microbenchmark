#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "bmb_verify.h"

void bmb_verify_matvec(bmb_verify_shape_t shape, int column_major, size_t m, size_t n,
                       const double *a, size_t lda,
                       const double *x, const double *xabs,
                       double *y, double *yabs)
{
    /* Element (r, c) of A, whichever way it is stored. */
#define BMB_A(r, c) (column_major ? a[(r) + (c) * lda] : a[(r) * lda + (c)])
    size_t rows = (shape == BMB_VERIFY_FULL) ? m : n;
    size_t i, j;

    for (i = 0; i < rows; i++) {
        double s = 0.0, sa = 0.0;

        switch (shape) {
        case BMB_VERIFY_FULL:
            for (j = 0; j < n; j++) {
                s += BMB_A(i, j) * x[j];
                sa += fabs(BMB_A(i, j)) * (xabs ? xabs[j] : fabs(x[j]));
            }
            break;
        case BMB_VERIFY_FULL_T:
            for (j = 0; j < m; j++) {
                s += BMB_A(j, i) * x[j];
                sa += fabs(BMB_A(j, i)) * (xabs ? xabs[j] : fabs(x[j]));
            }
            break;
        case BMB_VERIFY_UPPER:
            for (j = i; j < n; j++) {
                s += BMB_A(i, j) * x[j];
                sa += fabs(BMB_A(i, j)) * (xabs ? xabs[j] : fabs(x[j]));
            }
            break;
        case BMB_VERIFY_LOWER_STRICT:
            for (j = 0; j < i; j++) {
                s += BMB_A(i, j) * x[j];
                sa += fabs(BMB_A(i, j)) * (xabs ? xabs[j] : fabs(x[j]));
            }
            break;
        case BMB_VERIFY_SYM_UPPER:
            for (j = 0; j < n; j++) {
                double aij = (j >= i) ? BMB_A(i, j) : BMB_A(j, i);

                s += aij * x[j];
                sa += fabs(aij) * (xabs ? xabs[j] : fabs(x[j]));
            }
            break;
        }
        y[i] = s;
        yabs[i] = sa;
    }
#undef BMB_A
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
