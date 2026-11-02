#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./double/bmb_daxpby daxpby -v 8
bmb_check_run ./double/bmb_daxpby daxpby 3 -x 1 -i 2 -v 8:32
