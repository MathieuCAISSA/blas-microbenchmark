#!/bin/sh
# Every allocation a benchmark makes can fail, and each failure ends the
# run cleanly: exit status 1 and "out of memory" on stderr, never a crash
# and never a run that passes short. Each benchmark is run once to count
# its own allocations (setup, --verify's check, the timing buffer, the
# results, with -o), then once per allocation, that one made to fail by
# libbmb_failalloc.so (bmb_failalloc.c), preloaded; dgemm once more under
# --cache cold, on a machine whose last-level cache takes it 3 copies of
# its operands: the list of copies, and a copy's setup with the others
# already made; and, where the library has cblas_?gemm_batch, a batch of
# dgemm in a group call, whose arrays of pointers are allocations too
# (#159). SKIPs where it cannot be preloaded: under
# AddressSanitizer, whose allocator must come first, and outside glibc.
set -e
# bmb_missing: an extension this build's library does not have (#171).
. "${srcdir:-.}/test_helper.sh"

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
probe=level3/double/bmb_dgemm
if ldd "$probe" 2>/dev/null | grep -q 'libasan\.'; then
    echo "SKIP: built with AddressSanitizer, whose allocator cannot be preloaded over"
    exit 77
fi
# counted FILE: the count the library put on stderr, or nothing.
# The most any process made: under --star, each instance counts on from
# the calling process's count where it forked.
counted() {
    sed -n 's/^bmb_failalloc: \([0-9][0-9]*\) allocations$/\1/p' "$1" | sort -n | tail -n 1
}
LD_PRELOAD=$L BMB_ALLOC_COUNT=1 "$probe" -x 0 -i 1 -m 8 >/dev/null 2>"$T/err" || true
if [ -z "$(counted "$T/err")" ]; then
    echo "SKIP: $L could not be preloaded (glibc only)"
    exit 77
fi

# each_allocation BENCHMARK ARGS...: counts the allocations of a run, then
# makes each fail in turn. Leaves the count in $count.
each_allocation() {
    b=$1
    shift
    LD_PRELOAD=$L BMB_ALLOC_COUNT=1 "$b" "$@" -o "$T/out.json" >/dev/null 2>"$T/err" \
        || fail "$b $* failed with no allocation made to fail: $(cat "$T/err")"
    count=$(counted "$T/err")
    # The context and at least one operand, in setup().
    test "$count" -ge 2 || fail "$b $*: only $count allocations counted"
    i=1
    while [ "$i" -le "$count" ]; do
        status=0
        LD_PRELOAD=$L BMB_FAIL_ALLOC=$i "$b" "$@" -o "$T/out.json" >/dev/null 2>"$T/err" \
            || status=$?
        test "$status" -eq 1 \
            || fail "$b $*, allocation $i of $count failing: exit status $status, not 1: $(cat "$T/err")"
        grep -qi 'out of memory' "$T/err" \
            || fail "$b $*, allocation $i of $count failing: no \"out of memory\" on stderr: $(cat "$T/err")"
        i=$((i + 1))
        runs=$((runs + 1))
    done
}

n=0
runs=0
for b in level1/*/bmb_* level2/*/bmb_* level3/*/bmb_*; do
    case $b in *.*) continue ;; esac
    test -x "$b" || continue
    # An extension the library lacks; a call it is known to get wrong
    # (bmb_known_wrong), which test_bmb_verify.sh holds to its message.
    known=$(bmb_known_wrong "$b" "${b##*/bmb_}")
    if bmb_missing "${b##*/bmb_}" || { [ -n "$known" ] && [ "${known#maybe }" = "$known" ]; }; then
        n=$((n + 1))
        continue
    fi
    case $b in level1/*) flag=-v ;; *) flag=-m ;; esac
    each_allocation "$b" -x 0 -i 1 "$flag" 16
    n=$((n + 1))
done

# A 64K last-level cache: dgemm at 64 takes (64*64 + 2*64*64) * 8 = 98304
# bytes, 2 other copies and the one in use. Each copy is a context and
# its three matrices.
C=$T/machine/sys/devices/system/cpu/cpu0/cache/index0
mkdir -p "$C"
printf '2\n' >"$C/level"
printf 'Unified\n' >"$C/type"
printf '64K\n' >"$C/size"
BMB_MACHINE_ROOT=$PWD/$T/machine
export BMB_MACHINE_ROOT
cold=$runs
each_allocation level3/double/bmb_dgemm -x 0 -i 1 -k cold -m 64
unset BMB_MACHINE_ROOT
test "$count" -ge 13 || fail "dgemm -k cold: $count allocations, not the list and 3 copies of 4"
cold=$((runs - cold))

batch=$runs
if grep -q '^#define HAVE_CBLAS_GEMM_BATCH 1$' "${top_builddir:?top_builddir is not set}/config.h"; then
    each_allocation level3/double/bmb_dgemm -x 0 -i 1 -m 8 --batch-count 3 --batch-mode group
fi
batch=$((runs - batch))

# --star (#173): an allocation fails in every instance at once, which all
# skip the point, and in the calling process after they started; where
# two instances cannot run, not tried.
star=$runs
if BMB_COUNTS=1 level1/double/bmb_ddot --star -t 2 -v 16 >/dev/null 2>&1; then
    each_allocation level1/double/bmb_ddot -x 0 -i 1 -v 16 --star -t 2
fi
star=$((runs - star))

test "$n" -eq "$(grep -c '^[a-z]' "${srcdir:-.}/routines.txt")" || fail "$n benchmarks found, not the routines of routines.txt"
rm -rf "$T"
echo "ok   $n benchmarks, dgemm under --cache cold ($cold) and in a group call ($batch), and ddot under --star ($star), each of their $runs allocations made to fail in turn: exit 1, \"out of memory\", no crash"
