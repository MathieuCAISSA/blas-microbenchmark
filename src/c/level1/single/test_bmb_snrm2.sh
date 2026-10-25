#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./single/bmb_snrm2 snrm2 3 -x 1 -i 2 -v 8:32
