#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./complex/bmb_cgemmt cgemmt -m 8
bmb_known_wrong_run ./complex/bmb_cgemmt cgemmt -m 8
bmb_check_run ./complex/bmb_cgemmt cgemmt 4 -x 1 -i 2 -m 8:16 -M 8:16
