/* Unit tests for the building blocks of --verify: the reference products
 * on small matrices worked out by hand, real and complex, in double and
 * single precision, the comparison and its message, the tolerances, the
 * probe vector, and the corruption hooks the benchmark
 * tests use to prove each check can fail.
 *
 * Every benchmark's own check is tested end to end by
 * src/c/test_bmb_verify.sh; this pins the arithmetic they all rest on. */
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_verify.h"

static int failures;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static int same(const double *got, const double *want, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (got[i] != want[i]) {
            return 0;
        }
    }
    return 1;
}

/* Row-major, with a leading dimension of 4 so that every shape has to
 * honour lda rather than assume it is the width. The 9s below the diagonal
 * are what the upper shapes must not read. */
static const double square[12] = {
    1, 2, 3, -100,
    9, 4, 5, -100,
    9, 9, 6, -100,
};

static void test_matvec(void)
{
    static const double wide[8] = {
        1, 2, 3, -100,
        4, 5, 6, -100,
    };
    const double x3[3] = {1, -1, 2}, x2[2] = {1, -1}, ones[3] = {1, 1, 1};
    double y[3], yabs[3];

    bmb_verify_matvec(BMB_VERIFY_FULL, 0, 2, 3, wide, 4, x3, NULL, y, yabs);
    check(same(y, (double[]) {5, 11}, 2), "FULL: A*x on a 2x3 matrix with lda 4");
    check(same(yabs, (double[]) {9, 21}, 2), "FULL: |A|*|x|");

    bmb_verify_matvec(BMB_VERIFY_FULL_T, 0, 2, 3, wide, 4, x2, NULL, y, yabs);
    check(same(y, (double[]) {-3, -3, -3}, 3), "FULL_T: A'*x");
    check(same(yabs, (double[]) {5, 7, 9}, 3), "FULL_T: |A'|*|x|");

    bmb_verify_matvec(BMB_VERIFY_UPPER, 0, 0, 3, square, 4, ones, NULL, y, yabs);
    check(same(y, (double[]) {6, 9, 6}, 3), "UPPER: the strictly lower part is not read");

    bmb_verify_matvec(BMB_VERIFY_SYM_UPPER, 0, 0, 3, square, 4, ones, NULL, y, yabs);
    check(same(y, (double[]) {6, 11, 14}, 3), "SYM_UPPER: the lower half mirrored from the upper");

    bmb_verify_matvec(BMB_VERIFY_LOWER_STRICT, 0, 0, 3, square, 4, ones, NULL, y, yabs);
    check(same(y, (double[]) {0, 9, 18}, 3), "LOWER_STRICT: only below the diagonal");

    /* The size of the terms carries through a product of products. */
    bmb_verify_matvec(BMB_VERIFY_FULL, 0, 2, 3, wide, 4, x3, (double[]) {10, 10, 10}, y, yabs);
    check(same(yabs, (double[]) {60, 150}, 2), "xabs, when given, replaces |x|");
}

/* --layout col: the same matrices stored column-major, with a leading
 * dimension larger than the column, give the same products, shape by
 * shape. A shape is of the matrix, so the upper triangle stays i <= j. */
static void test_matvec_column_major(void)
{
    static const double wide[8] = {
        1, 2, 3, -100,
        4, 5, 6, -100,
    };
    double square_c[12], wide_c[9];
    const double x3[3] = {1, -1, 2}, x2[2] = {1, -1};
    double y[3], yabs[3], want[3], wabs[3];
    const bmb_verify_shape_t shapes[] = {
        BMB_VERIFY_UPPER, BMB_VERIFY_SYM_UPPER, BMB_VERIFY_LOWER_STRICT
    };
    size_t i, j, s;
    int same_all = 1;

    for (i = 0; i < 12; i++) {
        square_c[i] = -200;
    }
    for (i = 0; i < 9; i++) {
        wide_c[i] = -200;
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            square_c[i + j * 4] = square[i * 4 + j];
        }
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            wide_c[i + j * 3] = wide[i * 4 + j];
        }
    }

    bmb_verify_matvec(BMB_VERIFY_FULL, 1, 2, 3, wide_c, 3, x3, NULL, y, yabs);
    check(same(y, (double[]) {5, 11}, 2) && same(yabs, (double[]) {9, 21}, 2),
          "column-major FULL: A*x on the same 2x3 matrix, lda 3");
    bmb_verify_matvec(BMB_VERIFY_FULL_T, 1, 2, 3, wide_c, 3, x2, NULL, y, yabs);
    check(same(y, (double[]) {-3, -3, -3}, 3), "column-major FULL_T: A'*x");

    for (s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++) {
        bmb_verify_matvec(shapes[s], 0, 0, 3, square, 4, x3, NULL, want, wabs);
        bmb_verify_matvec(shapes[s], 1, 0, 3, square_c, 4, x3, NULL, y, yabs);
        same_all = same_all && same(y, want, 3) && same(yabs, wabs, 3);
    }
    check(same_all, "column-major UPPER, SYM_UPPER, LOWER_STRICT: the same triangles as row-major");
}

/* The other precisions (#6): a float matrix, and complex ones, row- and
 * column-major, each pair (re, im). */
static void test_matvec_precisions(void)
{
    static const float wide_f[8] = {
        1, 2, 3, -100,
        4, 5, 6, -100,
    };
    /* A = [1+2i  3-i; i  2], row-major with lda 3, then column-major
     * with lda 2; -100 is padding no product may read. */
    static const double cz_row[12] = {
        1, 2, 3, -1, -100, -100,
        0, 1, 2, 0, -100, -100,
    };
    static const double cz_col[8] = {
        1, 2, 0, 1,
        3, -1, 2, 0,
    };
    const double x3[3] = {1, -1, 2}, cx[4] = {1, 1, 2, -1};
    /* (1+2i)(1+i) + (3-i)(2-i) = 4-2i; i(1+i) + 2(2-i) = 3-i; and the
     * sizes (1+2)(1+1) + (3+1)(2+1) = 18, (0+1)(1+1) + (2+0)(2+1) = 8. */
    const double cwant[4] = {4, -2, 3, -1}, cwabs[2] = {18, 8};
    float cf_row[12], cf_col[8];
    double y[4], yabs[2];
    size_t i;

    bmb_verify_matvec_single(0, 2, 3, wide_f, 4, x3, NULL, y, yabs);
    check(same(y, (double[]) {5, 11}, 2) && same(yabs, (double[]) {9, 21}, 2),
          "single: A*x and |A|*|x| on a float 2x3 matrix with lda 4");

    bmb_verify_matvec_complex(0, 2, 2, cz_row, 3, cx, NULL, y, yabs);
    check(same(y, cwant, 4) && same(yabs, cwabs, 2), "complex: A*x and (|re|+|im|) sizes, row-major, lda 3");
    bmb_verify_matvec_complex(1, 2, 2, cz_col, 2, cx, NULL, y, yabs);
    check(same(y, cwant, 4) && same(yabs, cwabs, 2), "complex: the same, column-major");

    for (i = 0; i < 12; i++) {
        cf_row[i] = (float) cz_row[i];
    }
    for (i = 0; i < 8; i++) {
        cf_col[i] = (float) cz_col[i];
    }
    bmb_verify_matvec_complex_single(0, 2, 2, cf_row, 3, cx, NULL, y, yabs);
    check(same(y, cwant, 4) && same(yabs, cwabs, 2), "single complex: the same, row-major");
    bmb_verify_matvec_complex_single(1, 2, 2, cf_col, 2, cx, NULL, y, yabs);
    check(same(y, cwant, 4) && same(yabs, cwabs, 2), "single complex: the same, column-major");

    bmb_verify_matvec_complex(0, 2, 2, cz_row, 3, cx, (double[]) {10, 10}, y, yabs);
    check(same(yabs, (double[]) {70, 30}, 2), "complex: xabs, when given, replaces |re|+|im| of x");
}

static void test_close(void)
{
    const double want[3] = {1.0, -2.0, 3.0}, scale[3] = {1.0, 2.0, 3.0};
    double got[3];
    char msg[256] = "";

    memcpy(got, want, sizeof(got));
    check(bmb_verify_close("v", got, want, scale, 3, 1e-12, msg, sizeof(msg)), "equal values pass");

    got[1] = -2.0 * (1.0 + 1e-14);
    check(bmb_verify_close("v", got, want, scale, 3, 1e-12, msg, sizeof(msg)), "a difference within tol*scale passes");

    got[2] = 3.0 + 1e-6;
    check(!bmb_verify_close("v", got, want, scale, 3, 1e-12, msg, sizeof(msg)), "a difference beyond tol*scale fails");
    check(strstr(msg, "element 2 of v") != NULL, "the message names the wrong element");

    memcpy(got, want, sizeof(got));
    got[0] = NAN;
    check(!bmb_verify_close("v", got, want, scale, 3, 1e-12, msg, sizeof(msg)), "NaN fails");

    memcpy(got, want, sizeof(got));
    check(bmb_verify_close("v", got, want, NULL, 3, 0.0, msg, sizeof(msg)), "exact comparison: equal passes");
    got[0] = nextafter(want[0], 2.0);
    check(!bmb_verify_close("v", got, want, NULL, 3, 0.0, msg, sizeof(msg)), "exact comparison: one ulp fails");

    got[0] = 5.0;
    check(!bmb_verify_close("the sum", got, want, scale, 1, 1e-12, msg, sizeof(msg))
          && strncmp(msg, "the sum is 5", 12) == 0, "a single value is named without an index");

    /* The worst offender is reported, not the first. */
    got[0] = 1.0 + 1e-9;
    got[1] = -2.0 + 1e-3;
    got[2] = 3.0;
    bmb_verify_close("v", got, want, scale, 3, 1e-12, msg, sizeof(msg));
    check(strstr(msg, "element 1 of v") != NULL, "the message names the worst element");
}

static void test_tolerance(void)
{
    check(bmb_verify_tolerance(1) < bmb_verify_tolerance(1000), "the tolerance grows with the number of terms");
    check(bmb_verify_tolerance(1) > 2.0e-16, "one term still allows a few roundings");
    /* A 65536-wide product sums 2^17 terms per element of its projection;
     * the tests corrupt a result by a relative 1e-6. */
    check(bmb_verify_tolerance(1u << 17) < 1e-6 / 1000,
          "2^17 terms (a 65536-wide product) stay 1000 times below a 1e-6 error");
    check(bmb_verify_tolerance_single(1) > 4.0e-6 && bmb_verify_tolerance_single(1) < 1.0e-5,
          "single: one term allows a few float roundings, about 5e-6");
    check(fabs(bmb_verify_tolerance_single(1000) / bmb_verify_tolerance(1000) - 536870912.0) < 1.0,
          "single: 2^29 times the double tolerance, the ratio of the two epsilons");
}

static void test_probe(void)
{
    double a[1000], b[1000];
    size_t i;
    int in_range = 1, pos = 0, neg = 0;

    bmb_verify_probe(a, 1000);
    bmb_verify_probe(b, 1000);
    for (i = 0; i < 1000; i++) {
        if (!(fabs(a[i]) >= 0.5 && fabs(a[i]) <= 1.0)) {
            in_range = 0;
        }
        pos += a[i] > 0;
        neg += a[i] < 0;
    }
    check(in_range, "probe: every magnitude in [0.5, 1]");
    check(pos > 300 && neg > 300, "probe: both signs, roughly evenly");
    check(same(a, b, 1000), "probe: deterministic, so a failure can be reproduced");
}

static void test_perturb(void)
{
    double v = 2.0;

    unsetenv("BMB_VERIFY_CORRUPT");
    bmb_verify_perturb(&v);
    check(v == 2.0, "perturb: nothing happens without BMB_VERIFY_CORRUPT");

    setenv("BMB_VERIFY_CORRUPT", "0", 1);
    bmb_verify_perturb(&v);
    check(v == 2.0, "perturb: BMB_VERIFY_CORRUPT=0 is off");

    setenv("BMB_VERIFY_CORRUPT", "1", 1);
    bmb_verify_perturb(&v);
    check(fabs(v - 2.0 - 3.0e-6) < 1e-12, "perturb: a relative 1e-6 (plus 1e-6) with BMB_VERIFY_CORRUPT=1");
    unsetenv("BMB_VERIFY_CORRUPT");
}

static void test_perturb_single(void)
{
    float v = 2.0f;

    unsetenv("BMB_VERIFY_CORRUPT");
    bmb_verify_perturb_single(&v);
    check(v == 2.0f, "perturb single: nothing happens without BMB_VERIFY_CORRUPT");

    setenv("BMB_VERIFY_CORRUPT", "1", 1);
    bmb_verify_perturb_single(&v);
    check(v == 5.0f, "perturb single: the value doubled, plus 1, with BMB_VERIFY_CORRUPT=1");
    unsetenv("BMB_VERIFY_CORRUPT");
}

int main(void)
{
    test_matvec();
    test_matvec_column_major();
    test_matvec_precisions();
    test_close();
    test_tolerance();
    test_probe();
    test_perturb();
    test_perturb_single();

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
