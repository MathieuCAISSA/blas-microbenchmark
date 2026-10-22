#!/bin/sh
# --cache cold (#160) on real benchmarks, over a generated machine whose
# last-level cache is a 64K L2 (an instruction cache above it does not
# count): the copies of the operands each point rotates over, enough
# that the others reach twice that cache, from what the operands take
# (level 3 says it, levels 1 and 2 half their bytes); 1 once the operands
# alone are past it; at most 16384, with one warning, on the x86
# fixture's 32M L3; in the text, the CSV and the JSON; a run of a routine
# whose operand is restored, checked by --verify, and every benchmark run
# cold and checked; refused where the cache size cannot be read. Tiny
# sizes only.
set -e

T=cache-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

rm -rf "$T"
C=$T/machine/sys/devices/system/cpu/cpu0/cache
mkdir -p "$C/index0" "$C/index1" "$C/index2"
printf '1\n' >"$C/index0/level"
printf 'Data\n' >"$C/index0/type"
printf '32K\n' >"$C/index0/size"
printf '2\n' >"$C/index1/level"
printf 'Unified\n' >"$C/index1/type"
printf '64K\n' >"$C/index1/size"
printf '3\n' >"$C/index2/level"
printf 'Instruction\n' >"$C/index2/type"
printf '64M\n' >"$C/index2/size"
M=$PWD/$T/machine

# copies FILE: the copies of every row of a JSON file, one per line.
copies() {
    sed -n 's/^ *"copies": \([0-9]*\)$/\1/p' "$1"
}

# ddot reads 2 vectors: bytes 16 n, half of it 8 n. Twice 64K is 131072:
# 16 others and the one in use at 1024, 2 and 1 at 8192, the operands
# alone at 16384.
BMB_MACHINE_ROOT=$M level1/double/bmb_ddot -x 0 -i 1 -k cold -v 1024,8192,16384 \
    -o "$T/ddot.json" >"$T/ddot.txt" 2>"$T/err" || fail "ddot -k cold failed: $(cat "$T/err")"
test "$(copies "$T/ddot.json" | tr '\n' ' ')" = "17 3 1 " \
    || fail "ddot -k cold: copies $(copies "$T/ddot.json" | tr '\n' ' '), expected 17 3 1"
grep -q '^    "cache": "cold",$' "$T/ddot.json" || fail "ddot -k cold: the JSON does not say cold"
grep -q '^# run: .*, cold cache$' "$T/ddot.txt" || fail "ddot -k cold: the run line does not say cold"
grep -v '^#' "$T/ddot.txt" | awk 'NR == 1 { exit !($NF == "copies") }' || fail "ddot -k cold: no copies column in the table"
grep -v '^#' "$T/ddot.txt" | awk 'NR == 2 { exit !($NF == 17) }' || fail "ddot -k cold: the table does not give 17 copies"
echo "ok   ddot: 17, 3 and 1 copies, said in the table, the run line and the JSON"

# dgemm declares what its operands take: (16*16 + 2*16*16) * 8 = 6144
# bytes, 22 others and the one in use. In CSV, the copies last.
BMB_MACHINE_ROOT=$M level3/double/bmb_dgemm -x 0 -i 1 -k cold -m 16 -o "$T/dgemm.csv" >/dev/null 2>"$T/err" \
    || fail "dgemm -k cold failed: $(cat "$T/err")"
header=$(grep -v '^#' "$T/dgemm.csv" | sed -n 1p)
row=$(grep -v '^#' "$T/dgemm.csv" | sed -n 2p)
test "${header##*,}" = copies || fail "dgemm -k cold: the CSV's last column is ${header##*,}, not copies"
test "${row##*,}" = 23 || fail "dgemm -k cold: ${row##*,} copies, expected 23"
echo "ok   dgemm: 23 copies, from what its operands take, last in the CSV"

# dtrsm restores its operand before every call: under --cache cold, after
# it instead, and the run is still checked and timed. (16*16 + 16*16) * 8
# = 4096 bytes: 32 others and the one in use.
BMB_MACHINE_ROOT=$M level3/double/bmb_dtrsm -x 2 -i 3 -c -k cold -m 16 -o "$T/dtrsm.json" >/dev/null 2>"$T/err" \
    || fail "dtrsm -k cold failed: $(cat "$T/err")"
test "$(copies "$T/dtrsm.json")" = 33 || fail "dtrsm -k cold: copies $(copies "$T/dtrsm.json"), expected 33"
echo "ok   dtrsm: 33 copies, verified, restored after each call"

# Every benchmark, cold and checked: each says what its operands take,
# and restores its copies so that each call still computes the right
# thing.
n=0
for b in level1/*/bmb_* level2/*/bmb_* level3/*/bmb_*; do
    case $b in *.*) continue ;; esac
    test -x "$b" || continue
    case $b in level1/*) flag=-v ;; *) flag=-m ;; esac
    BMB_MACHINE_ROOT=$M "$b" -x 1 -i 2 -c -k cold "$flag" 16 >/dev/null 2>"$T/err" \
        || fail "$b -k cold failed: $(cat "$T/err")"
    n=$((n + 1))
done
test "$n" -ge 32 || fail "only $n benchmarks found"
echo "ok   all $n benchmarks run cold, checked"

# Hot by default: said so, and no copies anywhere.
BMB_MACHINE_ROOT=$M level1/double/bmb_ddot -x 0 -i 1 -v 1024 -o "$T/hot.json" >"$T/hot.txt" 2>"$T/err" \
    || fail "ddot failed: $(cat "$T/err")"
grep -q '^    "cache": "hot",$' "$T/hot.json" || fail "ddot: the JSON does not say hot"
if grep -q '"copies"' "$T/hot.json" || grep -v '^#' "$T/hot.txt" | grep -q copies; then
    fail "ddot: copies written in a hot run"
fi
echo "ok   hot by default, without copies"

# The x86 fixture's L3 is 32M: vectors of 8 and 16 take more copies than
# the 16384 allowed, and the run says once, from the first, that those
# points are partly warm.
F=${srcdir:-.}/common/fixtures/machine/x86
BMB_MACHINE_ROOT=$F level1/double/bmb_ddot -x 0 -i 1 -k cold -v 8,16 -o "$T/tiny.json" >/dev/null 2>"$T/err" \
    || fail "ddot -k cold -v 8,16 failed: $(cat "$T/err")"
test "$(copies "$T/tiny.json" | tr '\n' ' ')" = "16384 16384 " \
    || fail "ddot -k cold -v 8,16: copies $(copies "$T/tiny.json" | tr '\n' ' '), expected 16384 16384"
test "$(grep -c 'partly warm' "$T/err")" -eq 1 || fail "ddot -k cold -v 8,16: not one warning: $(cat "$T/err")"
grep -q 'from Vector size 8 down' "$T/err" || fail "ddot -k cold -v 8,16: the warning does not start at 8: $(cat "$T/err")"
echo "ok   at most 16384 copies, and one warning past that"

# No cache size, no cold run: exit 1, and why.
status=0
BMB_MACHINE_ROOT=$PWD/$T/no-machine level1/double/bmb_ddot -x 0 -i 1 -k cold -v 8 >/dev/null 2>"$T/err" || status=$?
test "$status" -eq 1 || fail "-k cold without a cache size: exit status $status, not 1"
grep -q 'size of the last-level cache cannot be read' "$T/err" \
    || fail "-k cold without a cache size: not said why: $(cat "$T/err")"
echo "ok   refused where the cache size cannot be read"

rm -rf "$T"
echo "all checks passed"
