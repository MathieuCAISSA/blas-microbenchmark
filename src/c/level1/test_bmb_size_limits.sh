#!/bin/sh
# A size whose square wraps size_t must be refused by the parser, not
# turned into a small allocation the setup loop then overruns. 1518500250
# is the smallest such value: 1518500250^2 * 8 comes back down to 291 MB.
#
# Everything here must be *rejected*, because a rejection happens before
# anything is allocated. Do not add a case that runs a benchmark at a size
# near the limit to check it is accepted: a single vector at that size is
# 12 GB, Linux overcommits both of ddot's buffers, and touching them takes
# the machine down with the test.
set -e

fail() {
    echo "FAIL: $1"
    exit 1
}

refuse() {
    if ./bmb_ddot -v "$1" -i 1 >/dev/null 2>&1; then
        fail "-v $1 was accepted"
    fi
}

refuse 1518500250   # smallest size whose square wraps size_t
refuse 2147483648   # first size past INT_MAX
refuse 99999999999  # past every ceiling

# An ordinary size must still work.
./bmb_ddot -v 64 -i 1 >/dev/null || fail "-v 64 stopped working"

echo "ok   size limits"
