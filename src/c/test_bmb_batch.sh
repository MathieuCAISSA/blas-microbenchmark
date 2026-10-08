#!/bin/sh
# Checks that -b cannot batch the routines whose operand is restored before
# every call (dtrmv, dtrsv, dtrmm, dtrsm): N calls in a row would run on an
# operand drifting towards infinity or denormals, and the timing would stop
# meaning anything (#48). They stay at one call per sample, which -s shows
# as calls_per_sample, with a warning; another routine keeps the batch it
# was given, without one. Tiny sizes only.
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

for b in level2/bmb_dtrmv level2/bmb_dtrsv level3/bmb_dtrmm level3/bmb_dtrsm; do
    name=$(basename "$b")
    "$b" -C -x 0 -i 2 -b 50 -s -m 8,16 -o "$T/$name.json" >/dev/null 2>"$T/$name.err" \
        || fail "$name -b 50 failed: $(cat "$T/$name.err")"
    test "$(calls "$T/$name.json" | sort -u)" = 1 \
        || fail "$name -b 50 batched its calls: calls_per_sample $(calls "$T/$name.json" | tr '\n' ' ')"
    grep -q -- "--batch is ignored for ${name#bmb_}" "$T/$name.err" \
        || fail "$name -b 50 did not say -b was ignored: $(cat "$T/$name.err")"
done
echo "ok   -b 50: dtrmv, dtrsv, dtrmm and dtrsm stay at one call per sample, and say so"

level2/bmb_dgemv -C -x 0 -i 2 -b 50 -s -m 8 -o "$T/dgemv.json" >/dev/null 2>"$T/dgemv.err" \
    || fail "dgemv -b 50 failed: $(cat "$T/dgemv.err")"
test "$(calls "$T/dgemv.json")" = 50 || fail "dgemv -b 50: calls_per_sample $(calls "$T/dgemv.json")"
test ! -s "$T/dgemv.err" || fail "dgemv -b 50 warned: $(cat "$T/dgemv.err")"
level2/bmb_dtrsv -C -x 0 -i 2 -b 1 -m 8 >/dev/null 2>"$T/one.err" || fail "dtrsv -b 1 failed"
test ! -s "$T/one.err" || fail "dtrsv -b 1, which changes nothing, warned: $(cat "$T/one.err")"
echo "ok   another routine keeps -b 50, and -b 1 on dtrsv is not warned about"

rm -rf "$T"
echo "all checks passed"
