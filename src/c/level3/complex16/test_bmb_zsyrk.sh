#!/bin/sh
# zsyrk's smoke test, and the sizes whose complex matrices would wrap
# size_t, as for zgemm: the N x K one, or the N x N one when N x K still
# fits. Each must be refused before anything is allocated.
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./complex16/bmb_zsyrk zsyrk 4 -x 1 -i 2 -m 8:16 -M 8:16

bmb_check_too_large ./complex16/bmb_zsyrk zsyrk -m 1073741823 -M 1518500249
bmb_check_too_large ./complex16/bmb_zsyrk zsyrk -m 1073741825 -M 1
