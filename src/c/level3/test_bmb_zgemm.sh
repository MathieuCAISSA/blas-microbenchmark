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

bmb_check_too_large ./bmb_zgemm zgemm -m 1073741825 -M 1
# M x K fits, K x N does not: (2^30 - 1) x 1518500249 complex doubles.
bmb_check_too_large ./bmb_zgemm zgemm -m 1073741823 -M 1518500249
