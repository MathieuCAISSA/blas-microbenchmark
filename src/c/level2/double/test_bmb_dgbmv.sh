#!/bin/sh
# dgbmv's smoke test, and a K whose band, 2K + 1 diagonals, would not fit
# the int BLAS takes: the parser bounds K alone, so the benchmark refuses
# it before anything is allocated (one row of such a band is 24 GB).
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./double/bmb_dgbmv dgbmv 4 -x 1 -i 2 -m 8:16 -M 8:16

if ./double/bmb_dgbmv -m 1 -M 1518500249 -x 0 -i 1 -C >/dev/null 2>dgbmv-band.err; then
    bmb_fail "dgbmv: a band of 2K + 1 = 3037000499 diagonals was accepted"
fi
grep -q 'would not fit the int BLAS takes' dgbmv-band.err ||
    bmb_fail "dgbmv: a band too wide for an int was not refused for it: $(cat dgbmv-band.err)"
rm -f dgbmv-band.err
echo "ok   dgbmv refuses a band of 2K + 1 diagonals past an int"
