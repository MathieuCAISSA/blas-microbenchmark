#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./bmb_dswap dswap 3 -x 1 -i 2 -v 8:32
