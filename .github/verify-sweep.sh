#!/bin/sh
# Every benchmark, checked at realistic sizes, non-square shapes, 1, 2
# and 4 threads and, for those with a matrix, both storage orders
# (--layout row and col), against the library this CI job built with:
#
#     .github/verify-sweep.sh BUILD_DIR
#
# make check runs --verify on tiny sizes only, by rule (AGENTS.md, "Tiny
# sizes only"). A tolerance too tight for a real size, or a library that is
# only wrong once it splits the work between threads, would pass it. This
# is the same check at sizes a user measures, a few MB per operand, with
# the threads the runner has. No timing is worth anything on a CI runner:
# -i 1, and the numbers are thrown away.
set -e

B=${1:?usage: $0 BUILD_DIR}
failed=0
n=0

for f in "$B"/src/c/level1/bmb_* "$B"/src/c/level2/bmb_* "$B"/src/c/level3/bmb_*; do
    name=$(basename "$f")
    case $name in *.*) continue ;; esac
    [ -f "$f" ] && [ -x "$f" ] || continue
    case $f in
        */level1/*) sizes="-v 1000,100000,1000000" ;;
        */level2/*)
            case $name in
                bmb_dgemv | bmb_dger) sizes="-m 100,1000,2000 -M 37,1000" ;;
                *) sizes="-m 100,1000,2000" ;;
            esac
            ;;
        *) sizes="-m 64,256,512 -M 33,512" ;;
    esac
    case $f in
        */level1/*) layouts=- ;;
        *) layouts="row col" ;;
    esac
    for layout in $layouts; do
        lo=
        [ "$layout" = - ] || lo="--layout $layout"
        if "$f" -x 0 -i 1 -t 1,2,4 -c $lo $sizes >/dev/null 2>"$B/verify-sweep.err"; then
            echo "ok   $name $lo $sizes, 1 to 4 threads"
        else
            echo "FAIL $name $lo $sizes, 1 to 4 threads: $(cat "$B/verify-sweep.err")"
            failed=$((failed + 1))
        fi
    done
    n=$((n + 1))
done

rm -f "$B/verify-sweep.err"
test "$n" -ge 20 || { echo "only $n benchmarks found in $B"; exit 1; }
test "$failed" -eq 0 || { echo "$failed of $n benchmarks computed a wrong result"; exit 1; }
echo "all $n benchmarks verified"
