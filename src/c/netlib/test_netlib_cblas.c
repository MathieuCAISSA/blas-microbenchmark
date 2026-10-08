/* Checks the netlib CBLAS shim against naive reference implementations.
 *
 * The shim's risky part is translating row-major CBLAS calls into the
 * column-major Fortran ABI: a wrong flip computes a transposed or
 * mirror-triangle result silently, without crashing, which would quietly
 * benchmark the wrong thing. Everything here is row-major, since that is
 * what the benchmarks use. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cblas.h"

#define M 4
#define N 3
#define K 5
#define TOL 1e-9

static int failures;

static double val(int i)
{
    return ((i * 37) % 17) * 0.25 - 2.0;
}

static void fill(double *p, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        p[i] = val(i);
    }
}

/* Same, shifted, for routines that need two operands to differ. */
static void fill_off(double *p, int n, int off)
{
    int i;

    for (i = 0; i < n; i++) {
        p[i] = val(i + off);
    }
}

static void check(const char *name, const double *got, const double *want, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (fabs(got[i] - want[i]) > TOL * (1.0 + fabs(want[i]))) {
            printf("FAIL %-10s [%d]: got %.12g, want %.12g\n", name, i, got[i], want[i]);
            failures++;
            return;
        }
    }
    printf("ok   %s\n", name);
}

/* Row-major triangular matrix: dominant diagonal so triangular solves stay
 * well conditioned, 0.5 in the referenced triangle, and a value BLAS must
 * never look at in the other one -- filling it with zeros instead would let
 * a wrong Uplo flip in the shim quietly produce a plausible answer. */
static void fill_tri(double *a, int n, int upper)
{
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (i == j) {
                a[i * n + j] = 3.0;
            } else if ((j > i) == (upper != 0)) {
                a[i * n + j] = 0.5;
            } else {
                a[i * n + j] = -1.25;
            }
        }
    }
}

int main(void)
{
    double a[64], b[64], c[64], x[16], y[16], want[64], tmp[64];
    int i, j, l;

    /* ---- level 1 ---- */
    fill(x, 8);
    fill(y, 8);
    {
        double dot = 0.0, asum = 0.0, nrm = 0.0;

        for (i = 0; i < 8; i++) {
            dot += x[i] * y[i];
            asum += fabs(x[i]);
            nrm += x[i] * x[i];
        }
        nrm = sqrt(nrm);
        want[0] = dot;   check("ddot", (double[]){ cblas_ddot(8, x, 1, y, 1) }, want, 1);
        want[0] = asum;  check("dasum", (double[]){ cblas_dasum(8, x, 1) }, want, 1);
        want[0] = nrm;   check("dnrm2", (double[]){ cblas_dnrm2(8, x, 1) }, want, 1);
    }

    fill(x, 8);
    fill(y, 8);
    for (i = 0; i < 8; i++) {
        want[i] = 0.75 * x[i] + y[i];
    }
    cblas_daxpy(8, 0.75, x, 1, y, 1);
    check("daxpy", y, want, 8);

    /* ---- dgemv, NoTrans: y = alpha*A*x + beta*y, A is M x N row-major ---- */
    fill(a, M * N);
    fill(x, N);
    fill(y, M);
    for (i = 0; i < M; i++) {
        double acc = 0.0;

        for (j = 0; j < N; j++) {
            acc += a[i * N + j] * x[j];
        }
        want[i] = 0.5 * acc + 0.25 * y[i];
    }
    cblas_dgemv(CblasRowMajor, CblasNoTrans, M, N, 0.5, a, N, x, 1, 0.25, y, 1);
    check("dgemv.N", y, want, M);

    /* ---- dgemv, Trans: y = alpha*A^T*x + beta*y ---- */
    fill(a, M * N);
    fill(x, M);
    fill(y, N);
    for (j = 0; j < N; j++) {
        double acc = 0.0;

        for (i = 0; i < M; i++) {
            acc += a[i * N + j] * x[i];
        }
        want[j] = 0.5 * acc + 0.25 * y[j];
    }
    cblas_dgemv(CblasRowMajor, CblasTrans, M, N, 0.5, a, N, x, 1, 0.25, y, 1);
    check("dgemv.T", y, want, N);

    /* ---- dger: A += alpha*x*y^T ---- */
    fill(a, M * N);
    fill(x, M);
    fill(y, N);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            want[i * N + j] = a[i * N + j] + 0.5 * x[i] * y[j];
        }
    }
    cblas_dger(CblasRowMajor, M, N, 0.5, x, 1, y, 1, a, N);
    check("dger", a, want, M * N);

    /* ---- dsymv, Upper: only the upper triangle of A is referenced ---- */
    fill(a, M * M);
    fill(x, M);
    fill(y, M);
    for (i = 0; i < M; i++) {
        double acc = 0.0;

        for (j = 0; j < M; j++) {
            /* mirror the upper triangle to get the full symmetric matrix */
            acc += ((j >= i) ? a[i * M + j] : a[j * M + i]) * x[j];
        }
        want[i] = 0.5 * acc + 0.25 * y[i];
    }
    cblas_dsymv(CblasRowMajor, CblasUpper, M, 0.5, a, M, x, 1, 0.25, y, 1);
    check("dsymv.U", y, want, M);

    /* ---- dgemm, NoTrans/NoTrans: C = alpha*A*B + beta*C ---- */
    fill(a, M * K);
    fill(b, K * N);
    fill(c, M * N);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            double acc = 0.0;

            for (l = 0; l < K; l++) {
                acc += a[i * K + l] * b[l * N + j];
            }
            want[i * N + j] = 0.5 * acc + 0.25 * c[i * N + j];
        }
    }
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, M, N, K, 0.5, a, K, b, N, 0.25, c, N);
    check("dgemm.NN", c, want, M * N);

    /* ---- dgemm, Trans/NoTrans: C = alpha*A^T*B + beta*C, A is K x M ---- */
    fill(a, K * M);
    fill(b, K * N);
    fill(c, M * N);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            double acc = 0.0;

            for (l = 0; l < K; l++) {
                acc += a[l * M + i] * b[l * N + j];
            }
            want[i * N + j] = 0.5 * acc + 0.25 * c[i * N + j];
        }
    }
    cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans, M, N, K, 0.5, a, M, b, N, 0.25, c, N);
    check("dgemm.TN", c, want, M * N);

    /* ---- dsymm, Left/Upper: C = alpha*A*B + beta*C, A is M x M symmetric ---- */
    fill(a, M * M);
    fill(b, M * N);
    fill(c, M * N);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            double acc = 0.0;

            for (l = 0; l < M; l++) {
                acc += ((l >= i) ? a[i * M + l] : a[l * M + i]) * b[l * N + j];
            }
            want[i * N + j] = 0.5 * acc + 0.25 * c[i * N + j];
        }
    }
    cblas_dsymm(CblasRowMajor, CblasLeft, CblasUpper, M, N, 0.5, a, M, b, N, 0.25, c, N);
    check("dsymm.LU", c, want, M * N);

    /* ---- dsyrk, Upper/NoTrans: C = alpha*A*A^T + beta*C, A is N x K ----
     * Only C's upper triangle is written, so only that is compared. */
    fill(a, N * K);
    fill(c, N * N);
    memcpy(want, c, sizeof(double) * N * N);
    for (i = 0; i < N; i++) {
        for (j = i; j < N; j++) {
            double acc = 0.0;

            for (l = 0; l < K; l++) {
                acc += a[i * K + l] * a[j * K + l];
            }
            want[i * N + j] = 0.5 * acc + 0.25 * c[i * N + j];
        }
    }
    cblas_dsyrk(CblasRowMajor, CblasUpper, CblasNoTrans, N, K, 0.5, a, K, 0.25, c, N);
    for (i = 0; i < N; i++) {
        for (j = 0; j < i; j++) {   /* ignore the untouched lower triangle */
            want[i * N + j] = c[i * N + j];
        }
    }
    check("dsyrk.UN", c, want, N * N);

    /* ---- dtrmv, Upper/NoTrans/NonUnit: x = A*x ---- */
    fill_tri(a, M, 1);
    fill(x, M);
    for (i = 0; i < M; i++) {
        double acc = 0.0;

        for (j = i; j < M; j++) {
            acc += a[i * M + j] * x[j];
        }
        want[i] = acc;
    }
    cblas_dtrmv(CblasRowMajor, CblasUpper, CblasNoTrans, CblasNonUnit, M, a, M, x, 1);
    check("dtrmv.UN", x, want, M);

    /* ---- dtrsm, Left/Upper/NoTrans/NonUnit: solve A*X = alpha*B ----
     * Checked by substitution: multiplying the solution back must give
     * alpha*B. */
    fill_tri(a, M, 1);
    fill(b, M * N);
    memcpy(tmp, b, sizeof(double) * M * N);
    cblas_dtrsm(CblasRowMajor, CblasLeft, CblasUpper, CblasNoTrans, CblasNonUnit,
                M, N, 0.5, a, M, b, N);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            double acc = 0.0;

            for (l = i; l < M; l++) {
                acc += a[i * M + l] * b[l * N + j];
            }
            c[i * N + j] = acc;
            want[i * N + j] = 0.5 * tmp[i * N + j];
        }
    }
    check("dtrsm.LU", c, want, M * N);

    /* ---- level 1: the remaining routines the benchmarks call ---- */
    fill(x, 8);
    for (i = 0; i < 8; i++) {
        want[i] = 0.5 * x[i];
    }
    cblas_dscal(8, 0.5, x, 1);
    check("dscal", x, want, 8);

    fill(x, 8);
    fill_off(y, 8, 3);
    for (i = 0; i < 8; i++) {
        want[i] = x[i];
    }
    cblas_dcopy(8, x, 1, y, 1);
    check("dcopy", y, want, 8);

    fill(x, 8);
    fill_off(y, 8, 3);
    for (i = 0; i < 8; i++) {
        want[i] = val(i + 3);
        tmp[i] = val(i);
    }
    cblas_dswap(8, x, 1, y, 1);
    check("dswap.x", x, want, 8);
    check("dswap.y", y, tmp, 8);

    /* ---- dsyr, Upper: A += alpha*x*x^T, upper triangle only ----
     * want keeps A's original lower triangle, so a wrong Uplo flip (which
     * would update the lower one instead) fails on both triangles. */
    fill(a, M * M);
    fill(x, M);
    memcpy(want, a, sizeof(double) * M * M);
    for (i = 0; i < M; i++) {
        for (j = i; j < M; j++) {
            want[i * M + j] = a[i * M + j] + 0.5 * x[i] * x[j];
        }
    }
    cblas_dsyr(CblasRowMajor, CblasUpper, M, 0.5, x, 1, a, M);
    check("dsyr.U", a, want, M * M);

    /* ---- dsyr2, Upper: A += alpha*(x*y^T + y*x^T) ---- */
    fill(a, M * M);
    fill(x, M);
    fill_off(y, M, 3);
    memcpy(want, a, sizeof(double) * M * M);
    for (i = 0; i < M; i++) {
        for (j = i; j < M; j++) {
            want[i * M + j] = a[i * M + j] + 0.5 * (x[i] * y[j] + y[i] * x[j]);
        }
    }
    cblas_dsyr2(CblasRowMajor, CblasUpper, M, 0.5, x, 1, y, 1, a, M);
    check("dsyr2.U", a, want, M * M);

    /* ---- dtrmv, Lower/Trans/NonUnit: x = A^T*x ----
     * A^T of a lower-triangular A is upper-triangular, so a shim that
     * flipped only Uplo or only Trans lands on the wrong operand. */
    fill_tri(a, M, 0);
    fill(x, M);
    for (i = M - 1; i >= 0; i--) {
        double acc = 0.0;

        for (j = i; j < M; j++) {
            acc += a[j * M + i] * x[j];  /* (A^T)[i][j] = A[j][i], j >= i */
        }
        want[i] = acc;
    }
    cblas_dtrmv(CblasRowMajor, CblasLower, CblasTrans, CblasNonUnit, M, a, M, x, 1);
    check("dtrmv.LT", x, want, M);

    /* ---- dtrsv, Upper/NoTrans/NonUnit: solve A*x = b ----
     * Checked by substitution, like dtrsm below. */
    fill_tri(a, M, 1);
    fill(x, M);
    memcpy(tmp, x, sizeof(double) * M);
    cblas_dtrsv(CblasRowMajor, CblasUpper, CblasNoTrans, CblasNonUnit, M, a, M, x, 1);
    for (i = 0; i < M; i++) {
        double acc = 0.0;

        for (j = i; j < M; j++) {
            acc += a[i * M + j] * x[j];
        }
        y[i] = acc;
    }
    check("dtrsv.UN", y, tmp, M);

    /* ---- dsyr2k, Upper/NoTrans: C = alpha*(A*B^T + B*A^T) + beta*C ----
     * A and B are N x K row-major. */
    fill(a, N * K);
    fill_off(b, N * K, 3);
    fill(c, N * N);
    memcpy(want, c, sizeof(double) * N * N);
    for (i = 0; i < N; i++) {
        for (j = i; j < N; j++) {
            double acc = 0.0;

            for (l = 0; l < K; l++) {
                acc += a[i * K + l] * b[j * K + l] + b[i * K + l] * a[j * K + l];
            }
            want[i * N + j] = 0.5 * acc + 0.25 * c[i * N + j];
        }
    }
    cblas_dsyr2k(CblasRowMajor, CblasUpper, CblasNoTrans, N, K, 0.5, a, K, b, K, 0.25, c, N);
    check("dsyr2k.UN", c, want, N * N);

    /* ---- dtrmm, Left/Upper/NoTrans/NonUnit: B = alpha*A*B ----
     * The subtlest mapping in the shim: in row-major the triangular operand
     * swaps side and flips triangle, but TransA stays as it is. */
    fill_tri(a, M, 1);
    fill(b, M * N);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            double acc = 0.0;

            for (l = i; l < M; l++) {
                acc += a[i * M + l] * b[l * N + j];
            }
            want[i * N + j] = 0.5 * acc;
        }
    }
    cblas_dtrmm(CblasRowMajor, CblasLeft, CblasUpper, CblasNoTrans, CblasNonUnit,
                M, N, 0.5, a, M, b, N);
    check("dtrmm.LUN", b, want, M * N);

    /* ---- dtrmm, Left/Upper/Trans/NonUnit: B = alpha*A^T*B ----
     * Same call with TransA set, which is what pins down "transa stays". */
    fill_tri(a, M, 1);
    fill(b, M * N);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            double acc = 0.0;

            for (l = 0; l <= i; l++) {
                acc += a[l * M + i] * b[l * N + j];
            }
            want[i * N + j] = 0.5 * acc;
        }
    }
    cblas_dtrmm(CblasRowMajor, CblasLeft, CblasUpper, CblasTrans, CblasNonUnit,
                M, N, 0.5, a, M, b, N);
    check("dtrmm.LUT", b, want, M * N);

    /* ---- dsymm, Right/Upper: C = alpha*B*A + beta*C, A is N x N symmetric ----
     * The Left case above cannot catch a missing Side flip on its own. */
    fill(a, N * N);
    fill(b, M * N);
    fill(c, M * N);
    for (i = 0; i < M; i++) {
        for (j = 0; j < N; j++) {
            double acc = 0.0;

            for (l = 0; l < N; l++) {
                acc += b[i * N + l] * ((j >= l) ? a[l * N + j] : a[j * N + l]);
            }
            want[i * N + j] = 0.5 * acc + 0.25 * c[i * N + j];
        }
    }
    cblas_dsymm(CblasRowMajor, CblasRight, CblasUpper, M, N, 0.5, a, N, b, N, 0.25, c, N);
    check("dsymm.RU", c, want, M * N);

    /* ---- single precision: the values are multiples of 0.25, so these
     * small sums are exact in float too ---- */
    {
        float fa[64], fb[64], fc[64], fx[16], fy[16];
        double got[64];

        for (i = 0; i < 64; i++) {
            fa[i] = (float) val(i);
            fb[i] = (float) val(i + 3);
            fc[i] = (float) val(i + 5);
        }
        for (i = 0; i < 16; i++) {
            fx[i] = (float) val(i);
            fy[i] = (float) val(i + 3);
        }

        want[0] = 0.0;
        for (i = 0; i < 8; i++) {
            want[0] += (double) fx[i] * fy[i];
        }
        got[0] = cblas_sdot(8, fx, 1, fy, 1);
        check("sdot", got, want, 1);

        for (i = 0; i < 8; i++) {
            want[i] = 0.75 * fx[i] + fy[i];
        }
        cblas_saxpy(8, 0.75f, fx, 1, fy, 1);
        for (i = 0; i < 8; i++) {
            got[i] = fy[i];
        }
        check("saxpy", got, want, 8);

        /* sgemv, NoTrans, A M x N row-major */
        for (i = 0; i < M; i++) {
            double acc = 0.0;

            for (j = 0; j < N; j++) {
                acc += (double) fa[i * N + j] * fx[j];
            }
            want[i] = 0.5 * acc + 0.25 * fc[i];
        }
        cblas_sgemv(CblasRowMajor, CblasNoTrans, M, N, 0.5f, fa, N, fx, 1, 0.25f, fc, 1);
        for (i = 0; i < M; i++) {
            got[i] = fc[i];
        }
        check("sgemv.N", got, want, M);

        /* sgemm, NoTrans/NoTrans */
        for (i = 0; i < M * N; i++) {
            fc[i] = (float) val(i + 5);
        }
        for (i = 0; i < M; i++) {
            for (j = 0; j < N; j++) {
                double acc = 0.0;

                for (l = 0; l < K; l++) {
                    acc += (double) fa[i * K + l] * fb[l * N + j];
                }
                want[i * N + j] = 0.5 * acc + 0.25 * fc[i * N + j];
            }
        }
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, M, N, K, 0.5f, fa, K, fb, N, 0.25f, fc, N);
        for (i = 0; i < M * N; i++) {
            got[i] = fc[i];
        }
        check("sgemm.NN", got, want, M * N);
    }

    /* ---- zgemm and cgemm, ConjTrans/NoTrans and NoTrans/ConjTrans:
     * C = alpha*op(A)*op(B) + beta*C, complex alpha and beta. A conjugate
     * transpose is where a shim that flipped the flags of swapped
     * operands, as for the triangular routines, would go wrong. ---- */
    {
        double za[2 * 32], zb[2 * 32], zc[2 * 32], zw[2 * 32];
        const double alpha[2] = { 0.5, 0.25 }, beta[2] = { 0.25, -0.5 };
        float ca[2 * 32], cb[2 * 32], cc[2 * 32];
        const float falpha[2] = { 0.5f, 0.25f }, fbeta[2] = { 0.25f, -0.5f };
        double got[2 * 32];
        int pass;

        for (pass = 0; pass < 2; pass++) {
            /* pass 0: op(A) = A^H, A is K x M; pass 1: op(B) = B^H, B is N x K */
            fill(za, 2 * 32);
            fill_off(zb, 2 * 32, 3);
            fill_off(zc, 2 * 32, 5);
            for (i = 0; i < M; i++) {
                for (j = 0; j < N; j++) {
                    double sr = 0.0, si = 0.0, cr = zc[2 * (i * N + j)], ci = zc[2 * (i * N + j) + 1];

                    for (l = 0; l < K; l++) {
                        int ia = pass == 0 ? l * M + i : i * K + l;
                        int ib = pass == 0 ? l * N + j : j * K + l;
                        double ar = za[2 * ia], ai = za[2 * ia + 1];
                        double br = zb[2 * ib], bi = zb[2 * ib + 1];

                        if (pass == 0) {
                            ai = -ai;
                        } else {
                            bi = -bi;
                        }
                        sr += ar * br - ai * bi;
                        si += ar * bi + ai * br;
                    }
                    zw[2 * (i * N + j)] = alpha[0] * sr - alpha[1] * si + beta[0] * cr - beta[1] * ci;
                    zw[2 * (i * N + j) + 1] = alpha[0] * si + alpha[1] * sr + beta[0] * ci + beta[1] * cr;
                }
            }
            for (i = 0; i < 2 * 32; i++) {
                ca[i] = (float) za[i];
                cb[i] = (float) zb[i];
                cc[i] = (float) zc[i];
            }
            if (pass == 0) {
                cblas_zgemm(CblasRowMajor, CblasConjTrans, CblasNoTrans, M, N, K, alpha, za, M, zb, N,
                            beta, zc, N);
                cblas_cgemm(CblasRowMajor, CblasConjTrans, CblasNoTrans, M, N, K, falpha, ca, M, cb, N,
                            fbeta, cc, N);
            } else {
                cblas_zgemm(CblasRowMajor, CblasNoTrans, CblasConjTrans, M, N, K, alpha, za, K, zb, K,
                            beta, zc, N);
                cblas_cgemm(CblasRowMajor, CblasNoTrans, CblasConjTrans, M, N, K, falpha, ca, K, cb, K,
                            fbeta, cc, N);
            }
            check(pass == 0 ? "zgemm.CN" : "zgemm.NC", zc, zw, 2 * M * N);
            for (i = 0; i < 2 * M * N; i++) {
                got[i] = cc[i];
            }
            check(pass == 0 ? "cgemm.CN" : "cgemm.NC", got, zw, 2 * M * N);
        }
    }

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
