#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./complex16/bmb_zscal zscal 3 -x 1 -i 2 -v 8:32
