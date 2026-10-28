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
        # A family whose source it shares with another (bmb_symm.c and
        # bmb_hemm.c) includes it from a header of the level's own
        # directory, where the level is; a header of helpers it includes
        # too (bmb_packed.h, #244) declares none.
        src=$S/level$want/$f
        # Split on purpose: one header name a line, none with a space.
        for h in $(sed -n 's/^#include "\(bmb_[a-z0-9]*\.h\)"$/\1/p' "$src"); do
            if [ -f "$S/level$want/$h" ] && grep -q 'bench\.level = ' "$S/level$want/$h"; then
                src=$S/level$want/$h
            fi
        done
        got=$(sed -n 's/^ *bench\.level = \([0-9]*\);$/\1/p' "$src")
        test "$got" = "$want" || fail "$program's source, $f, declares level '$got', in level$want"
        n=$((n + 1))
    done <levels-sources.tmp
done
rm -f levels-sources.tmp
test "$n" -eq "$(grep -c '^[a-z]' "$S/routines.txt")" || fail "$n benchmarks found in the levels' Makefile.am, not the routines of routines.txt"
echo "ok   all $n benchmarks declare the level of their directory"
