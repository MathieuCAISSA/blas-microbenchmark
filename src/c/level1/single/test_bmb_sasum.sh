#!/bin/sh
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./single/bmb_sasum sasum 3 -x 1 -i 2 -v 8:32
