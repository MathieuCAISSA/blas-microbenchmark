#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_skip_missing ./complex16/bmb_zgemmt zgemmt -m 8
bmb_known_wrong_run ./complex16/bmb_zgemmt zgemmt -m 8
bmb_check_run ./complex16/bmb_zgemmt zgemmt 4 -x 1 -i 2 -m 8:16 -M 8:16

# The sizes whose complex matrices would wrap size_t, refused before
# anything is allocated, as zsyrk's: C N x N, A and B N x K.
bmb_check_too_large ./complex16/bmb_zgemmt zgemmt -m 1073741823 -M 1518500249
bmb_check_too_large ./complex16/bmb_zgemmt zgemmt -m 1073741825 -M 1
