#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./bmb_dger dger 4 -x 1 -i 2 -m 8:16 -M 8:16
