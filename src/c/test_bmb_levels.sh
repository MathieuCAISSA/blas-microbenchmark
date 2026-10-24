#!/bin/sh
# Every benchmark declares the BLAS level of the directory it is in
# (bench.level), which is the default size it takes when no size is given
# (#165): 10000000 at level 1, 5000 at level 2, 2000 at level 3. A level 2
# routine declared level 3 would measure 2000 x 2000 by default, where
# its matrix fits in a cache. Read from the source each program of the
# level's Makefile.am is built from, its own or its family's (#166), or
# the one that source includes, so nothing is run.
set -e

fail() {
    echo "FAIL: $*"
    exit 1
}

S=${srcdir:-.}
n=0
for want in 1 2 3; do
    sed -n 's/^\([a-z0-9_]*\)_SOURCES = \(.*\)$/\1 \2/p' "$S/level$want/Makefile.am" >levels-sources.tmp
    while read -r program f; do
        # A family that is another's with a flag (bmb_hemm.c, bmb_herk.c)
        # includes that one's source, where the level is.
        inc=$(sed -n 's/^#include "\(bmb_[a-z0-9]*\.c\)"$/\1/p' "$S/level$want/$f")
        src=$S/level$want/${inc:-$f}
        got=$(sed -n 's/^ *bench\.level = \([0-9]*\);$/\1/p' "$src")
        test "$got" = "$want" || fail "$program's source, $f, declares level '$got', in level$want"
        n=$((n + 1))
    done <levels-sources.tmp
done
rm -f levels-sources.tmp
test "$n" -ge 32 || fail "only $n benchmarks found in the levels' Makefile.am"
echo "ok   all $n benchmarks declare the level of their directory"
