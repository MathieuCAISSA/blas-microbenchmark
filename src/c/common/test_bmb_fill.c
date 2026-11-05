/* The fill most operands get (bmb_fill, bmb_prec.h) is the same bytes
 * whatever the build (#340): each element its multiply and its subtract
 * rounded apart, as the formula reads, an exact 0 every hundred elements.
 * A fused multiply-add, which GCC and Clang make of them by default where
 * the target has one, rounds once and changed the values; configure turns
 * contraction off (-ffp-contract=off). Compiled once in single precision
 * and once in double (BMB_PREC_S, BMB_PREC_D); a complex element's parts
 * use the same expression. It guards something where the compiler would
 * fuse: an aarch64 build, which always has FMA, or an x86 one built with
 * -march; and it fails when a user's CFLAGS turns contraction back on. */
#include <stdio.h>
#include <string.h>

#include "bmb_prec.h"

int main(void)
{
    enum { N = 300 };
    bmb_real_t x[BMB_PARTS * N];
    const bmb_real_t zero = BMB_R(0.0);
    int failures = 0;
    size_t i;

    bmb_fill(x, N, 7, 31);
    for (i = 0; i < N; i++) {
        /* volatile: the product stored, so rounded, before the subtract. */
        volatile bmb_real_t product = (bmb_real_t) ((i + 7) % 100) * BMB_R(0.01);
        const bmb_real_t want = product - BMB_R(0.5);

        /* The bytes, which is what every library must be given alike. */
        if (memcmp(&x[BMB_PARTS * i], &want, sizeof(want)) != 0) {
            printf("FAIL: element %zu is %.17g, not %.17g\n", i, (double) x[BMB_PARTS * i], (double) want);
            failures++;
        }
    }
    if (memcmp(&x[BMB_PARTS * 43], &zero, sizeof(zero)) != 0) {
        printf("FAIL: element 43, (43 + 7) %% 100 = 50, is %.17g, not an exact 0\n", (double) x[BMB_PARTS * 43]);
        failures++;
    }
    if (failures == 0) {
        printf("ok   %d elements of the fill, each rounded as the formula reads, an exact 0 among them\n", N);
    }
    return failures == 0 ? 0 : 1;
}
