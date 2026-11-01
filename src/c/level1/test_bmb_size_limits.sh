#!/bin/sh
# A size whose square wraps size_t must be refused by the parser, not
# turned into a small allocation the setup loop then overruns. 1518500250
# is the smallest such value: 1518500250^2 * 8 comes back down to 291 MB.
# A vector, in a build whose library takes 32-bit integers, stops below
# INT_MAX too; in an ILP64 build (#172) it may go past 2^31, up to what two
# vectors of 16-byte elements can be sized to.
#
# Everything run here must be *rejected*, because a rejection happens
# before anything is allocated, or run under BMB_COUNTS, which allocates
# nothing. Do not add a case that runs a benchmark at a size near the
# limit to check it is accepted: a single vector at that size is 12 GB,
# Linux overcommits both of ddot's buffers, and touching them takes the
# machine down with the test.
set -e

fail() {
    echo "FAIL: $1"
    exit 1
}

refuse() {
    if "$@" -i 1 >/dev/null 2>&1; then
        fail "$* was accepted"
    fi
}

refuse ../level2/double/bmb_dgemv -m 1518500250   # smallest size whose square wraps size_t
refuse ../level2/double/bmb_dgemv -m 99999999999  # past every ceiling

if grep -q '^#define BMB_ILP64 1$' "${top_builddir:?top_builddir is not set}/config.h"; then
    # Counted, never run: 2^31 elements are 16 GB of ddot's operands.
    BMB_COUNTS=1 ./double/bmb_ddot -v 2147483648 >/dev/null 2>counts.err || fail "-v 2147483648 was refused in an ILP64 build"
    grep -q '^counts: ddot 2147483648 ' counts.err || fail "-v 2147483648 was not counted at that size"
    rm -f counts.err
    refuse ./double/bmb_ddot -v 576460752303423488   # 2^59, past SIZE_MAX / 32
    echo "ok   ILP64: a vector past 2^31 taken (counted, not run), one past what size_t can size refused"
else
    refuse ./double/bmb_ddot -v 1518500250   # the vectors' ceiling is the matrices'
    refuse ./double/bmb_ddot -v 2147483648   # first size past INT_MAX
    refuse ./double/bmb_ddot -v 99999999999  # past every ceiling
fi

# An ordinary size must still work.
./double/bmb_ddot -v 64 -i 1 >/dev/null || fail "-v 64 stopped working"

echo "ok   size limits"
