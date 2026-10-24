#ifndef BMB_PREC_H
#define BMB_PREC_H

/* What a routine's precision changes in its source (#166): one source per
 * family of routines (bmb_gemm.c for sgemm, dgemm, cgemm and zgemm) is
 * compiled once per precision, with BMB_PREC_S, _D, _C or _Z defined,
 * which src/c/routines.sh writes into the level's Makefile.am from
 * src/c/routines.txt. Everything that differs between the precisions of a
 * family goes through what is here; what differs between families stays
 * in their source. */

#include <stddef.h>

#include "bmb_verify.h"

/* bmb_real_t: the real type, of a real element or of each part of a
 * complex one, real part first, the layout of C99's complex types and of
 * every CBLAS. BMB_PARTS: the reals per element. BMB_ELEM_BYTES: an
 * element's size, as a number the preprocessor can compare: the parser's
 * ceiling covers 8 bytes (bmb_bench_matrix_bytes), so only a routine of
 * larger elements checks its leading dimensions for itself. BMB_R(x): the
 * literal x in the real type, so that a fill computes in that type, as
 * the one-precision sources did. BMB_CBLAS(f): the CBLAS function of
 * family f; BMB_ROUTINE(f): its name. */
#if defined(BMB_PREC_S)
typedef float bmb_real_t;
#define BMB_COMPLEX 0
#define BMB_SINGLE 1
#define BMB_ELEM_BYTES 4
#define BMB_R(x) x##f
#define BMB_CBLAS(f) cblas_s##f
#define BMB_ROUTINE(f) "s" #f
#elif defined(BMB_PREC_D)
typedef double bmb_real_t;
#define BMB_COMPLEX 0
#define BMB_SINGLE 0
#define BMB_ELEM_BYTES 8
#define BMB_R(x) x
#define BMB_CBLAS(f) cblas_d##f
#define BMB_ROUTINE(f) "d" #f
#elif defined(BMB_PREC_C)
typedef float bmb_real_t;
#define BMB_COMPLEX 1
#define BMB_SINGLE 1
#define BMB_ELEM_BYTES 8
#define BMB_R(x) x##f
#define BMB_CBLAS(f) cblas_c##f
#define BMB_ROUTINE(f) "c" #f
#elif defined(BMB_PREC_Z)
typedef double bmb_real_t;
#define BMB_COMPLEX 1
#define BMB_SINGLE 0
#define BMB_ELEM_BYTES 16
#define BMB_R(x) x
#define BMB_CBLAS(f) cblas_z##f
#define BMB_ROUTINE(f) "z" #f
#else
#error "define one of BMB_PREC_S, BMB_PREC_D, BMB_PREC_C, BMB_PREC_Z"
#endif

#define BMB_PARTS (BMB_COMPLEX ? 2 : 1)

/* A scalar is stored as BMB_PARTS reals; BMB_SCALAR passes it as CBLAS
 * takes it: a real one by value, a complex one by its address. An array
 * is passed as void *, which converts to whatever pointer type a
 * library's cblas.h declares for it, complex or real. */
#if BMB_COMPLEX
#define BMB_SCALAR(s) ((const void *) (s))
#else
#define BMB_SCALAR(s) ((s)[0])
#endif

/* The FLOPs of one multiply-add: a complex one is 4 real multiplications
 * and 4 additions. */
#define BMB_MADD_FLOPS (BMB_COMPLEX ? 8.0 : 2.0)

/* --verify's references and tolerance for this precision, still computed
 * in double (bmb_verify.h). A single-precision result's tolerance counts
 * the library's terms alone, and its scale leaves out the rounding of the
 * result itself, which double precision's includes: see bmb_gemm.c. A
 * complex multiply-add counts as BMB_PARTS terms. */
#if BMB_COMPLEX && BMB_SINGLE
#define bmb_matvec bmb_verify_matvec_complex_single
#elif BMB_COMPLEX
#define bmb_matvec bmb_verify_matvec_complex
#elif BMB_SINGLE
#define bmb_matvec bmb_verify_matvec_single
#else
#define bmb_matvec bmb_verify_matvec
#endif
#if BMB_SINGLE
#define bmb_tolerance(terms) bmb_verify_tolerance_single(BMB_PARTS * (terms))
#define bmb_perturb bmb_verify_perturb_single
#else
#define bmb_tolerance(terms) bmb_verify_tolerance(BMB_PARTS * (terms))
#define bmb_perturb bmb_verify_perturb
#endif

/* What a complex check compares, in its message. */
#if BMB_COMPLEX
#define BMB_PARTS_NOTE " (real and imaginary parts in turn)"
#else
#define BMB_PARTS_NOTE ""
#endif

/* bmb_set_scalar(s, re, im): s = re + i im, im ignored for a real one. */
static inline void bmb_set_scalar(bmb_real_t *s, bmb_real_t re, bmb_real_t im)
{
    s[0] = re;
#if BMB_COMPLEX
    s[1] = im;
#else
    (void) im;
#endif
}

/* bmb_fill(x, count, re, im): count elements, the reals of each from i %
 * 100 shifted by re (the real part) and im (the imaginary part, ignored
 * for a real one), between -0.5 and 0.49: the fill every routine uses, so
 * that no two operands, or two parts, are the same. */
static inline void bmb_fill(bmb_real_t *x, size_t count, size_t re, size_t im)
{
    size_t i;

#if !BMB_COMPLEX
    (void) im;
#endif
    for (i = 0; i < count; i++) {
        x[BMB_PARTS * i] = (bmb_real_t) ((i + re) % 100) * BMB_R(0.01) - BMB_R(0.5);
#if BMB_COMPLEX
        x[2 * i + 1] = (bmb_real_t) ((i + im) % 100) * BMB_R(0.01) - BMB_R(0.5);
#endif
    }
}

/* bmb_scale(want, al, y, m): want = al * y, m elements of BMB_PARTS
 * reals each, al a scalar of the routine's precision. */
static inline void bmb_scale(double *want, const bmb_real_t *al, const double *y, size_t m)
{
    size_t i;

    for (i = 0; i < m; i++) {
#if BMB_COMPLEX
        want[2 * i] = al[0] * y[2 * i] - al[1] * y[2 * i + 1];
        want[2 * i + 1] = al[0] * y[2 * i + 1] + al[1] * y[2 * i];
#else
        want[i] = al[0] * y[i];
#endif
    }
}

/* |al|, as the scales of --verify bound it: |re| + |im| for a complex one,
 * which bounds each part of al * y by |al| |y|. */
static inline double bmb_abs(const bmb_real_t *al)
{
#if BMB_COMPLEX
    return (al[0] < 0 ? -al[0] : al[0]) + (al[1] < 0 ? -al[1] : al[1]);
#else
    return al[0] < 0 ? -al[0] : al[0];
#endif
}

#endif /* BMB_PREC_H */
