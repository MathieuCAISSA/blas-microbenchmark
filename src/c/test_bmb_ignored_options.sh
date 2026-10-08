#!/bin/sh
# An option a benchmark has no use for is ignored with a warning naming it,
# never silently: --layout on a vector routine (#7), --vector-size on a
# matrix one, --matrix-dim1 on a vector one, --matrix-dim2 on a routine
# with a single dimension. The run still succeeds. Where the option does
# apply, there is no such warning. Tiny sizes only.
set -e

T=ignored-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

rm -rf "$T"
mkdir "$T"

# warns <message> <benchmark> <options...>: runs it, which must succeed and
# say <message> on stderr. Only that message is looked for: a machine that
# exposes its CPU frequency adds a warning of its own to every run.
warns() {
    want=$1
    shift
    "$@" -C -x 0 -i 1 >/dev/null 2>"$T/err" || fail "$* failed: $(cat "$T/err")"
    grep -q -- "$want" "$T/err" || fail "$* did not warn \"$want\": $(cat "$T/err")"
}

warns '--layout is ignored for ddot: it has no matrix' level1/bmb_ddot -v 8 -L col
warns '--matrix-dim1 is ignored for ddot' level1/bmb_ddot -v 8 -m 8
warns '--vector-size is ignored for dgemv' level2/bmb_dgemv -m 8 -v 8
warns '--matrix-dim2 is ignored for dtrsv' level2/bmb_dtrsv -m 8 -M 8
echo "ok   an option a routine does not use is ignored with a warning naming it"

level2/bmb_dgemv -C -x 0 -i 1 -m 8 -M 8 -L col >/dev/null 2>"$T/err" || fail "dgemv -M -L failed"
if grep -q 'is ignored' "$T/err"; then
    fail "dgemv, which uses -M and -L, warned: $(cat "$T/err")"
fi
echo "ok   no warning where the options apply"

rm -rf "$T"
echo "all checks passed"
