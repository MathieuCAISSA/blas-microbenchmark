#!/bin/sh
# zgemv's smoke test, and a size where its M x N complex matrix would
# wrap size_t (2^30 + 1 by 2^30 + 1): the parser's ceiling is for real
# doubles. It must be refused before anything is allocated.
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./complex16/bmb_zgemv zgemv 4 -x 1 -i 2 -m 8:16 -M 8:16

bmb_check_too_large ./complex16/bmb_zgemv zgemv -m 1073741825 -M 1073741825
