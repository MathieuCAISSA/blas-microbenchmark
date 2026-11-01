#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./double/bmb_dgemmt dgemmt -m 8
bmb_known_wrong_run ./double/bmb_dgemmt dgemmt -m 8
bmb_check_run ./double/bmb_dgemmt dgemmt 4 -x 1 -i 2 -m 8:16 -M 8:16
