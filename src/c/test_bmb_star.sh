#!/bin/sh
# --star (#173): -t's counts as instances of one thread, each a process
# pinned to a core of its own, all measured at once. Small vectors only:
# every instance allocates its own operands.
set -e

fail() {
    echo "FAIL: $*"
    exit 1
}

T=star-test.$$
rm -rf "$T"
mkdir "$T"
trap 'rm -rf "$T"' EXIT

B=level1/double/bmb_ddot

# More instances than cores is refused, and the refusal says how many
# cores there are, which the rest of the test needs.
if $B --star -t 100000 -x 0 -w 0 -i 1 -v 64 >/dev/null 2>"$T/many.err"; then
    fail "--star -t 100000 was accepted"
fi
cores=`sed -n 's/.*may run on \([0-9][0-9]*\) core.*/\1/p' "$T/many.err"`
test -n "$cores" || fail "the refusal does not say how many cores: `cat "$T/many.err"`"
if [ "$cores" -lt 2 ]; then
    echo "SKIP: this process may run on one core only"
    exit 77
fi

# Two instances: the header says how they ran, the rows are at 2, and
# the JSON records the CPUs they were pinned to.
$B --star -t 2 -x 0 -w 0 -i 3 -v 4096 -o "$T/star.json" >"$T/star.txt" 2>"$T/star.err" \
    || fail "--star -t 2 failed: `cat "$T/star.err"`"
grep -q '^# star: ' "$T/star.txt" || fail "the text has no star line"
rows=`grep -c '^2  *4096 ' "$T/star.txt" || true`
test "$rows" -eq 1 || fail "the text has $rows rows at 2 instances, expected 1"
grep -q '"star": {"cpus": "[0-9][0-9,-]*"}' "$T/star.json" || fail "the JSON has no star block"

# The rate is the two instances' sum: twice ddot's 2n operations over the
# time the slowest took.
awk '/"time_s":/ { gsub(/[",]/, ""); t = $2 }
     /"gflops":/ { gsub(/[",]/, ""); g = $2 }
     END {
         want = 2 * 2 * 4096 / t / 1e9
         d = (g - want) / want
         if (d < 0) d = -d
         exit !(t > 0 && d < 1e-3)
     }' "$T/star.json" || fail "the GFLOP/s is not two instances' work over the time"

# Without -t, one instance a core.
$B --star -x 0 -w 0 -i 1 -v 64 >"$T/all.txt" 2>"$T/all.err" || fail "--star alone failed: `cat "$T/all.err"`"
n=`awk '$1 ~ /^[0-9]+$/ { print $1 }' "$T/all.txt"`
test "$n" = "$cores" || fail "--star alone ran $n instances, expected one on each of $cores cores"

# A wrong result is told once, by the first instance that found it, and
# ends the run as it does without --star.
set +e
BMB_VERIFY_CORRUPT=1 $B --star -t 2 -x 0 -w 0 -i 1 -v 64 >/dev/null 2>"$T/wrong.err"
status=$?
set -e
test "$status" -eq 2 || fail "a wrong result under --star exited $status, expected 2"
told=`grep -c 'Wrong result' "$T/wrong.err" || true`
test "$told" -eq 1 || fail "a wrong result was told $told times, expected once"
grep -q 'instance 0 of 2, on CPU [0-9]' "$T/wrong.err" || fail "the wrong result does not name its instance"

# BMB_COUNTS measures nothing, so starts nothing: one count a point.
BMB_COUNTS=1 $B --star -t 2 -v 64 >/dev/null 2>"$T/counts.err" || fail "BMB_COUNTS under --star failed"
counts=`grep -c '^counts:' "$T/counts.err" || true`
test "$counts" -eq 1 || fail "BMB_COUNTS under --star printed $counts counts, expected 1"

# Without --star, nothing of it.
$B -t 1 -x 0 -w 0 -i 1 -v 64 >"$T/plain.txt" || fail "the plain run failed"
if grep -q '^# star' "$T/plain.txt"; then
    fail "a run without --star has a star line"
fi

echo "ok   --star: $cores cores; instances refused past them, rates summed, a wrong result told once"
