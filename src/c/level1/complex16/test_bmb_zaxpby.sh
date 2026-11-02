#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./complex16/bmb_zaxpby zaxpby -v 8
bmb_check_run ./complex16/bmb_zaxpby zaxpby 3 -x 1 -i 2 -v 8:32
