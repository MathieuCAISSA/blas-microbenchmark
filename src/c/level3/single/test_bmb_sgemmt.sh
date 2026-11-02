#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./single/bmb_sgemmt sgemmt -m 8
bmb_known_wrong_run ./single/bmb_sgemmt sgemmt -m 8
bmb_check_run ./single/bmb_sgemmt sgemmt 4 -x 1 -i 2 -m 8:16 -M 8:16
