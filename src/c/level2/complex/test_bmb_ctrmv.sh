#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./complex/bmb_ctrmv ctrmv 3 -x 1 -i 2 -m 8:32
