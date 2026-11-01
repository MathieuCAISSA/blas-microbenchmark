#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./complex16/bmb_zgemmt zgemmt -m 8
bmb_check_run ./complex16/bmb_zgemmt zgemmt 4 -x 1 -i 2 -m 8:16 -M 8:16
