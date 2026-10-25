#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./complex/bmb_cher2 cher2 3 -x 1 -i 2 -m 8:32
