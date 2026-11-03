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
# 65536: a call long enough that the JSON's time, to the nanosecond,
# does not round the rate by more than the check allows.
$B --star -t 2 -x 0 -w 0 -i 3 -v 65536 -o "$T/star.json" >"$T/star.txt" 2>"$T/star.err" \
    || fail "--star -t 2 failed: `cat "$T/star.err"`"
grep -q '^# star: ' "$T/star.txt" || fail "the text has no star line"
rows=`grep -c '^2  *65536 ' "$T/star.txt" || true`
test "$rows" -eq 1 || fail "the text has $rows rows at 2 instances, expected 1"
grep -q '"star": {"cpus": "[0-9][0-9,-]*"}' "$T/star.json" || fail "the JSON has no star block"

# The rate is the two instances' sum: twice ddot's 2n operations over the
# time the slowest took.
awk '/"time_s":/ { gsub(/[",]/, ""); t = $2 }
     /"gflops":/ { gsub(/[",]/, ""); g = $2 }
     END {
         want = 2 * 2 * 65536 / t / 1e9
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

# A warning is told once, not by each instance: --cache cold short of a
# generated machine's 64M last-level cache at a tiny size.
C=$T/machine/sys/devices/system/cpu/cpu0/cache/index0
mkdir -p "$C"
printf '3\n' >"$C/level"
printf 'Unified\n' >"$C/type"
printf '64M\n' >"$C/size"
BMB_MACHINE_ROOT=$PWD/$T/machine $B --star -t 2 -x 0 -w 0 -i 1 -k cold -v 64 >/dev/null 2>"$T/cold.err" \
    || fail "--star --cache cold failed: `cat "$T/cold.err"`"
told=`grep -c 'partly warm' "$T/cold.err" || true`
test "$told" -eq 1 || fail "the partly warm warning was told $told times, expected once: `cat "$T/cold.err"`"

# BMB_COUNTS measures nothing, so starts nothing: one count a point.
BMB_COUNTS=1 $B --star -t 2 -v 64 >/dev/null 2>"$T/counts.err" || fail "BMB_COUNTS under --star failed"
counts=`grep -c '^counts:' "$T/counts.err" || true`
test "$counts" -eq 1 || fail "BMB_COUNTS under --star printed $counts counts, expected 1"

# An instance lost mid-run (killed) stops the others, which would wait
# for it forever: exit 1, said, none left running. -T 30 -E 30 make the
# run long enough to be caught at it: the sample cap ends it after about
# 8 s, the kill after 1. A run that still waits for the lost instance is
# stopped after 60 s and fails, rather than hanging the test.
if command -v pgrep >/dev/null 2>&1; then
    $B --star -t 2 -x 0 -w 0 -i 5 -T 30 -E 30 -v 64 >/dev/null 2>"$T/lost.err" &
    p=$!
    tries=0
    while [ "$(pgrep -P "$p" | wc -l)" -lt 2 ] && [ "$tries" -lt 50 ]; do
        sleep 1
        tries=$((tries + 1))
    done
    pids=`pgrep -P "$p" || true`
    victim=`printf '%s\n' $pids | tail -n 1`
    test -n "$victim" || fail "no instance of the long run found"
    kill -9 "$victim" 2>/dev/null || fail "the long run ended before an instance could be killed"
    tries=0
    while [ "$tries" -lt 60 ]; do
        case `ps -o stat= -p "$p" 2>/dev/null` in "" | Z*) break ;; esac
        sleep 1
        tries=$((tries + 1))
    done
    if [ "$tries" -eq 60 ]; then
        kill -9 "$p" $pids 2>/dev/null || true
        fail "a run whose instance was killed was still running 60 s later"
    fi
    status=0
    wait "$p" || status=$?
    test "$status" -eq 1 || fail "a run whose instance was killed exited $status, not 1: $(cat "$T/lost.err")"
    grep -q 'was killed by signal 9' "$T/lost.err" || fail "the lost instance was not said: $(cat "$T/lost.err")"
    # The others, gone with the run, not left to init: a moment for the
    # last of them to be reaped.
    tries=0
    for q in $pids; do
        while kill -0 "$q" 2>/dev/null && [ "$tries" -lt 5 ]; do
            sleep 1
            tries=$((tries + 1))
        done
        if kill -0 "$q" 2>/dev/null; then
            fail "instance $q was left running after one was lost"
        fi
    done
    echo "ok   an instance killed mid-run stops the run: exit 1, said, none left"
else
    echo "ok   (no pgrep: an instance lost mid-run not tried)"
fi

# Without --star, nothing of it.
$B -t 1 -x 0 -w 0 -i 1 -v 64 >"$T/plain.txt" || fail "the plain run failed"
if grep -q '^# star' "$T/plain.txt"; then
    fail "a run without --star has a star line"
fi

echo "ok   --star: $cores cores; instances refused past them, rates summed, a wrong result and a warning told once"
