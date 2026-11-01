#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./single/bmb_saxpby saxpby -v 8
bmb_check_run ./single/bmb_saxpby saxpby 3 -x 1 -i 2 -v 8:32
