#!/bin/sh
# Every allocation a benchmark makes can fail, and each failure ends the
# run cleanly: exit status 1 and "out of memory" on stderr, never a crash
# and never a run that passes short. Each benchmark is run once to count
# its own allocations (setup, --verify's check, the timing buffer, the
# results, with -o), then once per allocation, that one made to fail by
# libbmb_failalloc.so (bmb_failalloc.c), preloaded. SKIPs where it cannot
# be preloaded: under AddressSanitizer, whose allocator must come first,
# and outside glibc.
set -e

L=./libbmb_failalloc.so
T=alloc-failures-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

test -f "$L" || fail "no $L"
L=$(cd "$(dirname "$L")" && pwd)/$(basename "$L")
rm -rf "$T"
mkdir "$T"

# One that the library sees allocating, or this test checks nothing.
probe=level3/bmb_dgemm
if ldd "$probe" 2>/dev/null | grep -q 'libasan\.'; then
    echo "SKIP: built with AddressSanitizer, whose allocator cannot be preloaded over"
    exit 77
fi
# counted FILE: the count the library put on stderr, or nothing.
counted() {
    sed -n 's/^bmb_failalloc: \([0-9][0-9]*\) allocations$/\1/p' "$1"
}
LD_PRELOAD=$L BMB_ALLOC_COUNT=1 "$probe" -x 0 -i 1 -m 8 >/dev/null 2>"$T/err" || true
if [ -z "$(counted "$T/err")" ]; then
    echo "SKIP: $L could not be preloaded (glibc only)"
    exit 77
fi

n=0
runs=0
for b in level1/bmb_* level2/bmb_* level3/bmb_*; do
    case $b in *.*) continue ;; esac
    test -x "$b" || continue
    case $b in level1/*) size="-v 16" ;; *) size="-m 16" ;; esac
    # shellcheck disable=SC2086
    LD_PRELOAD=$L BMB_ALLOC_COUNT=1 "$b" -x 0 -i 1 $size -o "$T/out.json" >/dev/null 2>"$T/err" \
        || fail "$b failed with no allocation made to fail: $(cat "$T/err")"
    count=$(counted "$T/err")
    # The context and at least one operand, in setup().
    test "$count" -ge 2 || fail "$b: only $count allocations counted"
    i=1
    while [ "$i" -le "$count" ]; do
        status=0
        # shellcheck disable=SC2086
        LD_PRELOAD=$L BMB_FAIL_ALLOC=$i "$b" -x 0 -i 1 $size -o "$T/out.json" >/dev/null 2>"$T/err" \
            || status=$?
        test "$status" -eq 1 \
            || fail "$b, allocation $i of $count failing: exit status $status, not 1: $(cat "$T/err")"
        grep -qi 'out of memory' "$T/err" \
            || fail "$b, allocation $i of $count failing: no \"out of memory\" on stderr: $(cat "$T/err")"
        i=$((i + 1))
        runs=$((runs + 1))
    done
    n=$((n + 1))
done

test "$n" -ge 32 || fail "only $n benchmarks found"
rm -rf "$T"
echo "ok   $n benchmarks, each of their $runs allocations made to fail in turn: exit 1, \"out of memory\", no crash"
