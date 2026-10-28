#!/bin/sh
# An option a benchmark has no use for is ignored with a warning naming it,
# never silently: --layout and --ld-pad on a vector routine (#7, #163),
# --ld-pad on a packed matrix, which has no leading dimension and records
# no padding (#244), --vector-size on a matrix one, --matrix-dim1 on a vector one,
# --matrix-dim2 on a routine with a single dimension, --matrix-dim3 on
# any but GEMM (#215). An argument a routine does not have (--trans,
# --uplo, --side, --diag, #221), or a letter it does not take, is
# refused, never ignored: the run stops with a message naming it. The run
# still
# succeeds. Where the option does apply, there is no such warning. Tiny
# sizes only.
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

warns '--layout is ignored for ddot: it has no matrix' level1/double/bmb_ddot -v 8 -L col
warns '--ld-pad is ignored for ddot: it has no matrix' level1/double/bmb_ddot -v 8 -p 8
warns '--ld-pad is ignored for dspmv: its matrix is packed' level2/double/bmb_dspmv -m 8 -p 8
level2/double/bmb_dtpsv -C -x 0 -i 1 -m 8 -p 8 -o "$T/packed.json" >"$T/packed.out" 2>/dev/null ||
    fail "dtpsv -p 8 failed"
if grep -q 'leading dimensions' "$T/packed.out" || grep -q '"ld_pad"' "$T/packed.json"; then
    fail "dtpsv, whose matrix is packed, recorded a padding"
fi
grep -q '^  "layout": "row",$' "$T/packed.json" || fail "dtpsv, a matrix routine, records no layout"
warns '--matrix-dim1 is ignored for ddot' level1/double/bmb_ddot -v 8 -m 8
warns '--vector-size is ignored for dgemv' level2/double/bmb_dgemv -m 8 -v 8
warns '--matrix-dim2 is ignored for dtrsv' level2/double/bmb_dtrsv -m 8 -M 8
warns '--matrix-dim3 is ignored for dsyrk: only GEMM has a K apart' level3/double/bmb_dsyrk -m 8 -K 8
echo "ok   an option a routine does not use is ignored with a warning naming it"

level2/double/bmb_dgemv -C -x 0 -i 1 -m 8 -M 8 -L col -p 8 >/dev/null 2>"$T/err" || fail "dgemv -M -L -p failed"
if grep -q 'is ignored' "$T/err"; then
    fail "dgemv, which uses -M, -L and -p, warned: $(cat "$T/err")"
fi
level3/double/bmb_dgemm -C -x 0 -i 1 -m 8 -M 8 -K 8 >/dev/null 2>"$T/err" || fail "dgemm -K failed"
if grep -q 'is ignored' "$T/err"; then
    fail "dgemm, which uses -K, warned: $(cat "$T/err")"
fi
echo "ok   no warning where the options apply"

# refused <message> <benchmark> <options...>: runs it, which must fail and
# say <message>, measuring nothing.
refused() {
    want=$1
    shift
    if "$@" -C -x 0 -i 1 >"$T/out" 2>"$T/err"; then
        fail "$* was accepted"
    fi
    grep -q -- "$want" "$T/err" || fail "$* did not say \"$want\": $(cat "$T/err")"
    if grep -q '^[0-9]' "$T/out"; then
        fail "$* measured something before refusing"
    fi
}
refused '--trans: ddot has no matrix to transpose' level1/double/bmb_ddot -v 8 --trans T
refused '--uplo: dgemm has no triangle to choose' level3/double/bmb_dgemm -m 8 --uplo L
refused '--side: dgemv has no side to choose' level2/double/bmb_dgemv -m 8 --side R
refused '--diag: dgemm has no triangular matrix' level3/double/bmb_dgemm -m 8 --diag U
refused '--trans: dgemm takes two letters' level3/double/bmb_dgemm -m 8 --trans T
refused '--trans: dgemv takes one letter' level2/double/bmb_dgemv -m 8 --trans NT
refused '--trans: dgemv takes NT, not C' level2/double/bmb_dgemv -m 8 --trans C
refused '--trans: dgemm takes NT, not C' level3/double/bmb_dgemm -m 8 --trans NC
refused '--trans: zherk takes NC, not T' level3/complex16/bmb_zherk -m 8 --trans T
refused "--trans: dgemv takes NT, not C (a real matrix's conjugate transpose is T)" level2/double/bmb_dgemv -m 8 --trans C
refused '--trans: zsyrk takes NT, not C\.$' level3/complex16/bmb_zsyrk -m 8 --trans C
refused '--side: dsyrk has no side to choose' level3/double/bmb_dsyrk -m 8 --side R
refused '--diag: dsymv has no triangular matrix' level2/double/bmb_dsymv -m 8 --diag U
refused '--trans: dsymv has no matrix to transpose' level2/double/bmb_dsymv -m 8 --trans T
for args in "level3/double/bmb_dtrsm --side R --uplo L --trans T --diag U" "level3/double/bmb_dsymm --side R --uplo L" \
            "level3/double/bmb_dsyrk --uplo L --trans T" "level2/double/bmb_dtrsv --uplo L --trans T --diag U" \
            "level2/double/bmb_dsyr --uplo L"; do
    # shellcheck disable=SC2086 # the program and its options, split on purpose
    set -- $args
    b=$1
    shift
    "$b" -m 8 "$@" -C -x 0 -i 1 >/dev/null 2>"$T/err" || fail "$b $* was refused: $(cat "$T/err")"
done
echo "ok   an argument a routine does not have, or a letter it does not take, is refused; those it has are taken"

rm -rf "$T"
echo "all checks passed"
