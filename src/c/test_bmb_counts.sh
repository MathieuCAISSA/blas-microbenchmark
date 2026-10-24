#!/bin/sh
# Every routine's counts are the BLAS conventions, to the unit (#205): its
# FLOPs, the bytes of levels 1 and 2 (each operand read once, each result
# written once, as STREAM counts them) and the footprint of level 3 (the
# bytes of its operands), and none of those a routine must not declare:
# no FLOPs for copy and swap, no bytes at level 3, no footprint below it.
# Written here from the operation, the element's size and whether it is
# complex (a complex multiply-add is 4 real ones), independently of the
# sources, so that a family made of several routines (#166) keeps the
# counts each had. BMB_COUNTS makes a benchmark write each point's counts
# instead of measuring it: nothing is allocated, and one size is large.
set -e

T=counts-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

rm -rf "$T"
mkdir "$T"

# The sizes, odd and with dim1 and dim2 apart so that a formula that
# swaps or squares the wrong one shows; the last one large, past what an
# int-sized product would hold.
sizes() {
    case $1 in
        v) echo "-v 1000,4097,100000000" ;;
        n) echo "-m 30,61,100000" ;;
        *) echo "-m 30,61,100000 -M 50,7" ;;
    esac
}

grep -v '^#' "${srcdir:-.}/routines.txt" >"$T/routines" || fail "no routines in routines.txt"
n=0
while read -r r level prec kind _; do
    [ -n "$r" ] || continue
    case $prec in s) dir=single ;; d) dir=double ;; c) dir=complex ;; z) dir=complex16 ;; esac
    # shellcheck disable=SC2046 # the size options, split on purpose
    BMB_COUNTS=1 "level$level/$dir/bmb_$r" $(sizes "$kind") >"$T/$r.out" 2>"$T/$r.err" ||
        fail "bmb_$r failed under BMB_COUNTS: $(cat "$T/$r.err")"
    grep '^counts: ' "$T/$r.err" >"$T/$r.got" || fail "bmb_$r wrote no counts"
    if grep -q '^[0-9]' "$T/$r.out"; then
        fail "bmb_$r measured something under BMB_COUNTS: $(cat "$T/$r.out")"
    fi
    awk -v r="$r" -v prec="$prec" '
        BEGIN {
            op = substr(r, 2)
            e = prec == "s" ? 4 : prec == "z" ? 16 : 8
            cf = (prec == "c" || prec == "z") ? 4 : 1
        }
        function num(x) { return sprintf("%.17g", x) }
        {
            m = $3; n = $4; f = "-"; b = "-"; p = "-"
            if (op == "asum") { f = num(m); b = num(e * m) }
            else if (op == "axpy") { f = num(2 * m * cf); b = num(3 * e * m) }
            else if (op == "copy") { b = num(2 * e * m) }
            else if (op == "dot" || op == "dotc" || op == "dotu") { f = num(2 * m * cf); b = num(2 * e * m) }
            else if (op == "nrm2") { f = num(2 * m); b = num(e * m) }
            else if (op == "scal") { f = num(m); b = num(2 * e * m) }
            else if (op == "swap") { b = num(4 * e * m) }
            else if (op == "gemv") { f = num(2 * m * n * cf); b = num(e * (m * n + n + 2 * m)) }
            else if (op == "ger") { f = num(2 * m * n); b = num(e * (2 * m * n + m + n)) }
            else if (op == "symv") { f = num(2 * m * m); b = num(e * (m * (m + 1) / 2 + 3 * m)) }
            else if (op == "syr") { f = num(m * (m + 1)); b = num(e * (m * (m + 1) + m)) }
            else if (op == "syr2") { f = num(2 * m * (m + 1)); b = num(e * (m * (m + 1) + 2 * m)) }
            else if (op == "trmv" || op == "trsv") { f = num(m * (m + 1) * cf); b = num(e * (m * (m + 1) / 2 + 2 * m)) }
            else if (op == "gemm" || op == "symm" || op == "hemm") { f = num(2 * m * m * n * cf); p = num(e * (m * m + 2 * m * n)) }
            else if (op == "syrk" || op == "herk") { f = num(n * m * (m + 1) * cf); p = num(e * (m * n + m * m)) }
            else if (op == "syr2k") { f = num(2 * n * m * (m + 1) * cf); p = num(e * (2 * m * n + m * m)) }
            else if (op == "trmm" || op == "trsm") { f = num(n * m * (m + 1) * cf); p = num(e * (m * m + m * n)) }
            else { printf "no count for %s in this test: add its formula\n", r; bad = 1; exit 1 }
            want = sprintf("counts: %s %s %s flops %s bytes %s footprint %s", r, m, n, f, b, p)
            if ($0 != want) {
                printf "%s at %s x %s:\n  got  %s\n  want %s\n", r, m, n, $0, want
                bad = 1
            }
            points++
        }
        END { if (!bad && points == 0) { print r ": no point" ; bad = 1 } exit bad }
    ' "$T/$r.got" >"$T/$r.diff" || fail "$(cat "$T/$r.diff")"
    n=$((n + 1))
done <"$T/routines"
test "$n" -ge 32 || fail "only $n routines checked"
echo "ok   the FLOPs, bytes and footprint of all $n routines, at every size"

rm -rf "$T"
echo "all checks passed"
