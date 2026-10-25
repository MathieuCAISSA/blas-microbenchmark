#!/bin/sh
# zhemv's smoke test, and a size where its N x N complex matrix would
# wrap size_t (-m from 2^30 + 1): the parser's ceiling is for real
# doubles. It must be refused before anything is allocated.
set -e
. "${srcdir:-.}/../test_helper.sh"

bmb_check_run ./complex16/bmb_zhemv zhemv 3 -x 1 -i 2 -m 8:32

bmb_check_too_large ./complex16/bmb_zhemv zhemv -m 1073741825
