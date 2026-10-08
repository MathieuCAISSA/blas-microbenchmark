#!/bin/sh
# zgemm's smoke test, and a size where its complex matrices would wrap
# size_t: the parser's ceiling is for a matrix of real doubles, and from
# -m 1073741824 (2^30) an M x K complex one takes 2^64 bytes or more. At
# 2^30 + 1 they wrap to 32 GB, not to 0, which a check of the wrapped
# product alone would catch. It must be refused before anything is
# allocated, not turned into a buffer the setup loop overruns.
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./bmb_zgemm zgemm 4 -x 1 -i 2 -m 8:16 -M 8:16

if ./bmb_zgemm -m 1073741825 -M 1 -x 0 -i 1 -C >/dev/null 2>zgemm-wrap.err; then
    bmb_fail "zgemm: -m 1073741825 was accepted"
fi
# Its own message: a malloc of the 16 GB that K x N takes fails too on a
# smaller machine, and says only that setup failed.
grep -q 'more bytes than size_t can count' zgemm-wrap.err \
    || bmb_fail "zgemm: -m 1073741825 was not refused for its size: $(cat zgemm-wrap.err)"
rm -f zgemm-wrap.err
echo "ok   zgemm refuses a size whose complex matrix wraps size_t"
