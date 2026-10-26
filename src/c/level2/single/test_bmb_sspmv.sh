#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./single/bmb_sspmv sspmv 3 -x 1 -i 2 -m 8:32
