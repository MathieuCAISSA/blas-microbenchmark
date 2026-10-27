#!/bin/sh
# Checks that -b cannot batch the routines whose operand is restored
# before every call (the trmv, trsv, tpsv, tbsv, trmm and trsm families,
# every precision the table lists): N calls in a row would run on an
# operand drifting towards infinity or denormals, and the timing would
# stop meaning anything (#48). They stay at one call per sample, which
# the JSON gives as calls_per_sample and as the run block's batch, with
# a warning; another routine keeps the batch it was given, without one.
# Tiny sizes only.
set -e

T=batch-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

rm -rf "$T"
mkdir "$T"

# calls_per_sample of every row of a JSON file, one per line.
calls() {
    sed -n 's/^ *"calls_per_sample": \([0-9]*\),$/\1/p' "$1"
}

grep -v '^#' "${srcdir:-.}/routines.txt" | awk '$5 ~ /^(trmv|trsv|tpsv|tbsv|trmm|trsm)$/ { print $1, $2, $3 }' >"$T/reset" ||
    fail "no triangular routines in routines.txt"
n=0
while read -r r level prec; do
    case $prec in s) dir=single ;; d) dir=double ;; c) dir=complex ;; z) dir=complex16 ;; esac
    b=level$level/$dir/bmb_$r
    name=bmb_$r
    "$b" -C -x 0 -i 2 -b 50 -s -m 8,16 -o "$T/$name.json" >/dev/null 2>"$T/$name.err" \
        || fail "$name -b 50 failed: $(cat "$T/$name.err")"
    test "$(calls "$T/$name.json" | sort -u)" = 1 \
        || fail "$name -b 50 batched its calls: calls_per_sample $(calls "$T/$name.json" | tr '\n' ' ')"
    grep -q -- "--batch is ignored for ${name#bmb_}" "$T/$name.err" \
        || fail "$name -b 50 did not say -b was ignored: $(cat "$T/$name.err")"
    grep -q '^    "batch": 1,$' "$T/$name.json" \
        || fail "$name -b 50: the run block does not give the batch size used, 1"
    n=$((n + 1))
done <"$T/reset"
test "$n" -ge 20 || fail "only $n triangular routines found"
echo "ok   -b 50: the $n triangular multiplies and solves stay at one call per sample, say so, and record it"

level2/double/bmb_dgemv -C -x 0 -i 2 -b 50 -s -m 8 -o "$T/dgemv.json" >/dev/null 2>"$T/dgemv.err" \
    || fail "dgemv -b 50 failed: $(cat "$T/dgemv.err")"
test "$(calls "$T/dgemv.json")" = 50 || fail "dgemv -b 50: calls_per_sample $(calls "$T/dgemv.json")"
grep -q '^    "batch": 50,$' "$T/dgemv.json" || fail "dgemv -b 50: the run block does not give -b"
# Only this warning is looked for: on a machine that exposes its CPU
# frequency, every run may also warn about that.
if grep -q -- '--batch is ignored' "$T/dgemv.err"; then
    fail "dgemv -b 50 said -b was ignored: $(cat "$T/dgemv.err")"
fi
level2/double/bmb_dtrsv -C -x 0 -i 2 -b 1 -m 8 >/dev/null 2>"$T/one.err" || fail "dtrsv -b 1 failed"
if grep -q -- '--batch is ignored' "$T/one.err"; then
    fail "dtrsv -b 1, which changes nothing, said -b was ignored: $(cat "$T/one.err")"
fi
echo "ok   another routine keeps -b 50, and -b 1 on dtrsv is not warned about"

rm -rf "$T"
echo "all checks passed"
