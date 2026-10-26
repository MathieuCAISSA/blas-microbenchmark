#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./complex/bmb_cher2k cher2k 4 -x 1 -i 2 -m 8:16 -M 8:16
