#!/bin/sh
# ztrsm's smoke test, and the sizes whose complex matrices would wrap
# size_t, as for zgemm: the M x M one (-m from 2^30 + 1), or the M x N one
# when M x M still fits. Each must be refused before anything is
# allocated.
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./bmb_ztrsm ztrsm 4 -x 1 -i 2 -m 8:16 -M 8:16

bmb_check_too_large ./bmb_ztrsm ztrsm -m 1073741825 -M 1
bmb_check_too_large ./bmb_ztrsm ztrsm -m 1073741823 -M 1518500249
