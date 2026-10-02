#!/bin/sh
# Without -M, a two-dimension routine measures square matrices: dim2 follows
# dim1 point by point. It used to copy -m's *range* instead and measure the
# whole cross product -- nine runs for `-m 8:32`, where --help promised
# three square ones.
set -e
. "${srcdir:-.}/../test_helper.sh"

# Three points, and every one of them square.
bmb_check_run ./bmb_dgemm dgemm 3 -x 0 -i 1 -m 8:32
./bmb_dgemm -x 0 -i 1 -m 8:32 | grep -v '^#' | awk '
    NR == 1 { next }
    $2 != $3 { printf "FAIL: dgemm -m 8:32 measured %s x %s, not a square\n", $2, $3; bad = 1 }
    END { exit bad ? 1 : 0 }
' || exit 1

# With -M, every combination: the grid.
bmb_check_run ./bmb_dgemm dgemm 9 -x 0 -i 1 -m 8:32 -M 8:32

echo "ok   square default"
