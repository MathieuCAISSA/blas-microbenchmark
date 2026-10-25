#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./complex/bmb_caxpy caxpy 3 -x 1 -i 2 -v 8:32
