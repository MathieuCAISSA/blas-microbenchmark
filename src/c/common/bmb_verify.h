#ifndef BMB_VERIFY_H
#define BMB_VERIFY_H

#include <stddef.h>

/* Checking a benchmark's result against a reference (--verify).
 *
 * Each routine's verify() makes one call on the operands the benchmark is
 * about to time, then compares what the library computed with what it
 * should have. Levels 1 and 2 are recomputed directly, which costs about
 * one call. Level 3 is checked by random projection (Freivalds): C*r
 * against A*(B*r) for a vector r, which takes O(N^2) where recomputing C
 * would take O(N^3) -- and a wrong C gives a wrong C*r for all but a
 * vanishing set of r.
 *
 * Every reference comes with the size of the terms that made it, |A||x|
 * where the result is A*x, so that the tolerance is a rounding-error bound
 * on that element rather than a guess: a correct library lands well
 * inside it, a wrong one far outside. */

/* The matrix shapes a reference product reads, stored row- or
 * column-major with a leading dimension lda (see bmb_verify_matvec). */
typedef enum {
    BMB_VERIFY_FULL,      /* m x n */
    BMB_VERIFY_FULL_T,    /* the transpose of an m x n matrix */
    BMB_VERIFY_UPPER,     /* n x n upper triangular, diagonal included; the
                             strictly lower part is not read */
    BMB_VERIFY_SYM_UPPER, /* n x n symmetric, stored in its upper triangle */
    BMB_VERIFY_LOWER_STRICT /* n x n, its strictly lower part only: where a
                               routine that writes the upper triangle must
                               leave everything as it was */
} bmb_verify_shape_t;

/* y = op(A) x, and yabs = |op(A)| xabs, the size of the terms that made
 * each element of y (xabs NULL: |x|). FULL takes x of n elements and gives
 * y of m; FULL_T the other way round; the square shapes ignore m. A is
 * read as the benchmark stores it: column_major 0, element (i, j) at
 * a[i * lda + j]; 1, at a[i + j * lda]. The shapes are of the matrix, not
 * of the storage, so "upper" means i <= j either way. */
void bmb_verify_matvec(bmb_verify_shape_t shape, int column_major, size_t m, size_t n,
                       const double *a, size_t lda,
                       const double *x, const double *xabs,
                       double *y, double *yabs);

/* The FULL product of bmb_verify_matvec for the other precisions, still
 * computed in double. _single reads a float matrix. _complex reads an
 * m x n complex matrix, each element a (real, imaginary) pair, and takes
 * x and gives y the same way (n and m pairs); lda counts elements, not
 * pairs. xabs and yabs stay real, one per element: |re| + |im|, which
 * bounds the size of the two real products in each part of a complex
 * product. */
void bmb_verify_matvec_single(int column_major, size_t m, size_t n,
                              const float *a, size_t lda,
                              const double *x, const double *xabs,
                              double *y, double *yabs);
void bmb_verify_matvec_complex(int column_major, size_t m, size_t n,
                               const double *a, size_t lda,
                               const double *x, const double *xabs,
                               double *y, double *yabs);
void bmb_verify_matvec_complex_single(int column_major, size_t m, size_t n,
                                      const float *a, size_t lda,
                                      const double *x, const double *xabs,
                                      double *y, double *yabs);

/* The tolerance, relative to the size of the terms, for a result whose
 * elements each went through `terms` multiply-adds, counting those of the
 * reference: a rounding-error bound with room to spare. A complex
 * multiply-add counts as two: each part of it sums two real products. */
double bmb_verify_tolerance(size_t terms);

/* The same bound for a single-precision result, whose `terms` are the
 * library's alone: the reference, made in double, adds 2^-29 as much.
 * It grows with the terms as the double one does, from 4.8e-6 for one
 * term to 0.95 for a 10^6-element sdot, where it catches only a result
 * that is grossly wrong: a library summing 10^6 floats one after the
 * other can be off by a relative 1e-3, and a tighter guess would fail
 * it (doc/dev/benchmarks.md). */
double bmb_verify_tolerance_single(size_t terms);

/* 1 when |got[i] - want[i]| <= tol * scale[i] for every i; a NULL scale
 * asks for exact equality. Otherwise 0, and msg says which element of
 * `what` is wrong, by how much, and how much was allowed. */
int bmb_verify_close(const char *what, const double *got, const double *want,
                     const double *scale, size_t n, double tol,
                     char *msg, size_t size);

/* A deterministic vector for the random projection: magnitudes in
 * [0.5, 1], signs mixed, so that no element of the result is weighted
 * down to nothing. */
void bmb_verify_probe(double *x, size_t n);

/* Proving the checks can fail: when the environment sets
 * BMB_VERIFY_CORRUPT, each verify() alters one element of the library's
 * result by a relative 1e-6 before comparing it, and has to report it.
 * A testing aid; it has no other use. */
void bmb_verify_perturb(double *value);

/* The same for a single-precision result, which it doubles, plus one:
 * 1e-6 is a few units in the last place of a float, and the tolerance of
 * a float projection, as sgemm's and cgemm's, does not see even 1e-1 in
 * one element of a 64 x 64 C. A library that is wrong is usually wrong
 * everywhere, and by more. */
void bmb_verify_perturb_single(float *value);

#endif /* BMB_VERIFY_H */
