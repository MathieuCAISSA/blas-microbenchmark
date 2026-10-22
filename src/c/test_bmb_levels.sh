#!/bin/sh
# Every benchmark declares the BLAS level of the directory it is in
# (bench.level), which is the default size it takes when no size is given
# (#165): 10000000 at level 1, 5000 at level 2, 2000 at level 3. A level 2
# routine declared level 3 would measure 2000 x 2000 by default, where
# its matrix fits in a cache. Read from the sources, so nothing is run.
set -e

fail() {
    echo "FAIL: $*"
    exit 1
}

S=${srcdir:-.}
n=0
for f in "$S"/level1/*/bmb_*.c "$S"/level2/*/bmb_*.c "$S"/level3/*/bmb_*.c; do
    case $f in */level1/*) want=1 ;; */level2/*) want=2 ;; *) want=3 ;; esac
    got=$(sed -n 's/^ *bench\.level = \([0-9]*\);$/\1/p' "$f")
    test "$got" = "$want" || fail "$(basename "$f") declares level '$got', in level$want"
    n=$((n + 1))
done
test "$n" -ge 32 || fail "only $n benchmark sources found in $S"
echo "ok   all $n benchmarks declare the level of their directory"
