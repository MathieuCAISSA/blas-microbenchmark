#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./complex/bmb_caxpby caxpby -v 8
bmb_check_run ./complex/bmb_caxpby caxpby 3 -x 1 -i 2 -v 8:32
