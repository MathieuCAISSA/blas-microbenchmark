/* The fill most operands get (bmb_fill, bmb_prec.h) is the same whatever
 * the build (#340): each element its multiply and its subtract rounded
 * apart, as the formula reads, an exact 0 every hundred elements. A fused
 * multiply-add, which GCC and Clang make of them by default where the
 * target has one (-march, Spack's target flags), rounds once and changed
 * the values; configure turns contraction off (-ffp-contract=off).
 * Compiled once in single precision and once in double (BMB_PREC_S,
 * BMB_PREC_D). */
#include <stdio.h>

#include "bmb_prec.h"

int main(void)
{
    enum { N = 300 };
    bmb_real_t x[BMB_PARTS * N];
    int failures = 0;
    size_t i;

    bmb_fill(x, N, 7, 31);
    for (i = 0; i < N; i++) {
        /* volatile: each product stored, so rounded, before the subtract. */
        volatile bmb_real_t re = (bmb_real_t) ((i + 7) % 100) * BMB_R(0.01);
        bmb_real_t want = re - BMB_R(0.5);

        if (x[BMB_PARTS * i] != want) {
            printf("FAIL: element %zu is %.17g, not %.17g\n", i, (double) x[BMB_PARTS * i], (double) want);
            failures++;
        }
#if BMB_COMPLEX
        {
            volatile bmb_real_t im = (bmb_real_t) ((i + 31) % 100) * BMB_R(0.01);

            if (x[2 * i + 1] != im - BMB_R(0.5)) {
                printf("FAIL: element %zu's imaginary part is %.17g\n", i, (double) x[2 * i + 1]);
                failures++;
            }
        }
#endif
    }
    if (x[BMB_PARTS * 43] != BMB_R(0.0)) {
        printf("FAIL: element 43, (43 + 7) %% 100 = 50, is %.17g, not an exact 0\n", (double) x[BMB_PARTS * 43]);
        failures++;
    }
    if (failures == 0) {
        printf("ok   %d elements of the fill, each rounded as the formula reads, an exact 0 among them\n", N);
    }
    return failures == 0 ? 0 : 1;
}
