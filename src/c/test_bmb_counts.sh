#!/bin/sh
# Every routine's counts are the BLAS conventions, to the unit (#205):
# its FLOPs, the bytes of levels 1 and 2 (each operand read once, each
# result written once, as STREAM counts them) and the footprint of level
# 3 (the bytes of its operands), and none of those a routine must not
# declare: no FLOPs for copy, swap and iamax, no bytes at level 3, no
# footprint below it. Written here from the operation, the element's
# size and whether it is complex (a complex multiply-add is 4 real ones,
# a complex alpha times an element 6; asum and nrm2 count each real
# part), independently of the sources, so that a family made of several
# routines (#166) keeps the counts each had; GEMM's with a K of its own
# too (-K, #215). The operation is the routine's family, from the table:
# scasum is asum. BMB_COUNTS makes a benchmark write each point's counts
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
while read -r r level prec kind family; do
    [ -n "$r" ] || continue
    case $prec in s) dir=single ;; d) dir=double ;; c) dir=complex ;; z) dir=complex16 ;; esac
    # shellcheck disable=SC2046 # the size options, split on purpose
    BMB_COUNTS=1 "level$level/$dir/bmb_$r" $(sizes "$kind") >"$T/$r.out" 2>"$T/$r.err" ||
        fail "bmb_$r failed under BMB_COUNTS: $(cat "$T/$r.err")"
    grep '^counts: ' "$T/$r.err" >"$T/$r.got" || fail "bmb_$r wrote no counts"
    if grep -q '^[0-9]' "$T/$r.out"; then
        fail "bmb_$r measured something under BMB_COUNTS: $(cat "$T/$r.out")"
    fi
    awk -v r="$r" -v prec="$prec" -v op="$family" '
        BEGIN {
            e = prec == "s" ? 4 : prec == "z" ? 16 : 8
            cf = (prec == "c" || prec == "z") ? 4 : 1
            parts = cf == 4 ? 2 : 1
        }
        function num(x) { return sprintf("%.17g", x) }
        {
            m = $3; n = $4; f = "-"; b = "-"; p = "-"
            if (op == "asum") { f = num(m * parts); b = num(e * m) }
            else if (op == "axpy") { f = num(2 * m * cf); b = num(3 * e * m) }
            else if (op == "copy") { b = num(2 * e * m) }
            else if (op == "dot" || op == "dotc" || op == "dotu") { f = num(2 * m * cf); b = num(2 * e * m) }
            else if (op == "nrm2") { f = num(2 * m * parts); b = num(e * m) }
            else if (op == "scal") { f = num(parts == 2 ? 6 * m : m); b = num(2 * e * m) }
            else if (op == "swap") { b = num(4 * e * m) }
            else if (op == "iamax") { b = num(e * m) }
            else if (op == "rot") { f = num(6 * m); b = num(4 * e * m) }
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

# GEMM with a K of its own (-K, #215): 2*M*N*K and the bytes of M x K,
# K x N and M x N, the point saying its K.
for r in sgemm dgemm cgemm zgemm; do
    case $r in s*) dir=single e=4 cf=1 ;; d*) dir=double e=8 cf=1 ;; c*) dir=complex e=8 cf=4 ;; z*) dir=complex16 e=16 cf=4 ;; esac
    BMB_COUNTS=1 "level3/$dir/bmb_$r" -m 30,100000 -M 50,7 -K 3,90,100000 >"$T/$r.out" 2>"$T/$r.err" ||
        fail "bmb_$r -K failed under BMB_COUNTS: $(cat "$T/$r.err")"
    grep '^counts: ' "$T/$r.err" >"$T/$r.got" || fail "bmb_$r -K wrote no counts"
    awk -v r="$r" -v e="$e" -v cf="$cf" '
        function num(x) { return sprintf("%.17g", x) }
        {
            m = $3; n = $4; k = $6
            want = sprintf("counts: %s %s %s K %s flops %s bytes - footprint %s", r, m, n, k,
                           num(2 * m * n * k * cf), num(e * (m * k + k * n + m * n)))
            if ($5 != "K" || $0 != want) { printf "%s:\n  got  %s\n  want %s\n", r, $0, want; bad = 1 }
            points++
        }
        END { if (!bad && points != 12) { print r ": " points " points, not 12"; bad = 1 } exit bad }
    ' "$T/$r.got" >"$T/$r.diff" || fail "$(cat "$T/$r.diff")"
done
echo "ok   GEMM under -K: 2*M*N*K, and the bytes of M x K, K x N and M x N, at every K"

# A batch (#159): the call's counts are the batch's, its count times one
# matrix's, whichever mode passes them.
BMB_COUNTS=1 level3/double/bmb_dgemm -m 30 -M 50 -K 7 --batch-count 5 >/dev/null 2>"$T/batch.err" ||
    fail "dgemm --batch-count 5 failed under BMB_COUNTS: $(cat "$T/batch.err")"
grep -q '^counts: dgemm 30 50 K 7 flops 105000 bytes - footprint 82400$' "$T/batch.err" ||
    fail "dgemm --batch-count 5: $(grep '^counts' "$T/batch.err"), not 5 times 2*30*50*7 and (30*7 + 7*50 + 30*50)*8"
echo "ok   a batch of five: five times one matrix's counts"

# GEMV transposed (#221): the same FLOPs, x of M elements read and y of N
# read and written.
for r in sgemv dgemv cgemv zgemv; do
    case $r in s*) dir=single e=4 cf=1 ;; d*) dir=double e=8 cf=1 ;; c*) dir=complex e=8 cf=4 ;; z*) dir=complex16 e=16 cf=4 ;; esac
    BMB_COUNTS=1 "level2/$dir/bmb_$r" -m 30,61 -M 50,7 --trans T >/dev/null 2>"$T/$r.err" ||
        fail "bmb_$r --trans T failed under BMB_COUNTS: $(cat "$T/$r.err")"
    grep '^counts: ' "$T/$r.err" >"$T/$r.got" || fail "bmb_$r --trans T wrote no counts"
    awk -v r="$r" -v e="$e" -v cf="$cf" '
        { m = $3; n = $4
          want = sprintf("counts: %s %s %s flops %.17g bytes %.17g footprint -", r, m, n, 2 * m * n * cf, e * (m * n + m + 2 * n))
          if ($0 != want) { printf "%s:\n  got  %s\n  want %s\n", r, $0, want; bad = 1 }
          points++ }
        END { if (!bad && points != 4) { print r ": " points " points, not 4"; bad = 1 } exit bad }
    ' "$T/$r.got" >"$T/$r.diff" || fail "$(cat "$T/$r.diff")"
done
echo "ok   GEMV transposed: x of M elements, y of N"

# On the right side (#222), A is N x N: trmm's and trsm's M*N*(N+1),
# symm's and hemm's 2*M*N*N, and A's N*N bytes in the footprint.
grep -v '^#' "${srcdir:-.}/routines.txt" | awk '$5 ~ /^(trmm|trsm|symm|hemm)$/ { print $1, $3, $5 }' >"$T/right" ||
    fail "no right-side routines in routines.txt"
n=0
while read -r r prec family; do
    case $prec in s) dir=single e=4 cf=1 ;; d) dir=double e=8 cf=1 ;; c) dir=complex e=8 cf=4 ;; z) dir=complex16 e=16 cf=4 ;; esac
    BMB_COUNTS=1 "level3/$dir/bmb_$r" -m 30,61,100000 -M 50,7 --side R >/dev/null 2>"$T/$r.err" ||
        fail "bmb_$r --side R failed under BMB_COUNTS: $(cat "$T/$r.err")"
    grep '^counts: ' "$T/$r.err" >"$T/$r.got" || fail "bmb_$r --side R wrote no counts"
    awk -v r="$r" -v e="$e" -v cf="$cf" -v family="$family" '
        { m = $3; n = $4
          if (family == "trmm" || family == "trsm") { f = m * n * (n + 1) * cf; p = e * (n * n + m * n) }
          else { f = 2 * m * n * n * cf; p = e * (n * n + 2 * m * n) }
          want = sprintf("counts: %s %s %s flops %.17g bytes - footprint %.17g", r, m, n, f, p)
          if ($0 != want) { printf "%s:\n  got  %s\n  want %s\n", r, $0, want; bad = 1 }
          points++ }
        END { if (!bad && points != 6) { print r ": " points " points, not 6"; bad = 1 } exit bad }
    ' "$T/$r.got" >"$T/$r.diff" || fail "$(cat "$T/$r.diff")"
    n=$((n + 1))
done <"$T/right"
test "$n" -ge 6 || fail "only $n right-side routines checked"
echo "ok   the $n routines with a side, on the right: A N x N"

rm -rf "$T"
echo "all checks passed"
