#!/bin/sh
# Every benchmark, checked at realistic sizes, non-square shapes, 1, 2
# and 4 threads and, for those with a matrix, both storage orders
# (--layout row and col) and padded leading dimensions (column-major,
# --ld-pad 8, #163), against the library this CI job built with:
#
#     .github/scripts/verify-sweep.sh BUILD_DIR
#
# make check runs --verify on tiny sizes only, by rule (AGENTS.md, "Tiny
# sizes only"). A tolerance too tight for a real size, or a library that is
# only wrong once it splits the work between threads, would pass it. This
# is the same check at sizes a user measures, a few MB per operand, with
# the threads the runner has. No timing is worth anything on a CI runner:
# -i 1, no warm-up (-w 0, which a check does not need and would cost
# 0.2 s a thread count), and the numbers are thrown away.
#
# The same for the routines' arguments (#277): each value other than its
# default, one at a time, over the routines that take it (a transposed
# GEMM, a lower or right-side triangle, herk with C), GEMM with a K of
# its own (-K) and batched in each --batch-mode, in both storage orders
# at 1 and 4 threads, at one size past those where libraries switch
# kernels. make check runs every combination of them, at sizes below
# those. A batch mode the library has not is a skip.
set -e

B=${1:?usage: $0 BUILD_DIR}
failed=0
n=0

# check WHAT ARG...: the benchmark $f, run with --verify and ARG..., said
# ok, skip (exit 77) or FAIL.
check() {
    what=$1
    shift
    rc=0
    "$f" -x 0 -w 0 -i 1 -c "$@" >/dev/null 2>"$B/verify-sweep.err" || rc=$?
    case $rc in
        0) echo "ok   $name $what" ;;
        77) echo "skip $name $what: $(tail -n 1 "$B/verify-sweep.err")" ;;
        *)
            echo "FAIL $name $what: $(cat "$B/verify-sweep.err")"
            failed=$((failed + 1))
            ;;
    esac
}

for f in "$B"/src/c/level1/*/bmb_* "$B"/src/c/level2/*/bmb_* "$B"/src/c/level3/*/bmb_*; do
    name=$(basename "$f")
    case $name in *.*) continue ;; esac
    [ -f "$f" ] && [ -x "$f" ] || continue
    case $f in
        */level1/*) sizes="-v 1000,100000,1000000" ;;
        */level2/*)
            case $name in
                bmb_?gemv | bmb_?ger | bmb_?geru | bmb_?gerc) sizes="-m 100,1000,2000 -M 37,1000" ;;
                # -M is a band's K: narrow, wide, and past N
                bmb_?gbmv | bmb_?tbsv) sizes="-m 100,1000,2000 -M 1,37,2000" ;;
                *) sizes="-m 100,1000,2000" ;;
            esac
            ;;
        *) sizes="-m 64,256,512 -M 33,512" ;;
    esac
    case $f in
        */level1/*) layouts=- ;;
        */bmb_?spmv | */bmb_?tpsv) layouts="row col" ;;
        *) layouts="row col col:8" ;;
    esac
    for variant in $layouts; do
        layout=${variant%%:*}
        lo=
        [ "$layout" = - ] || lo="--layout $layout"
        [ "$variant" = "$layout" ] || lo="$lo --ld-pad ${variant#*:}"
        # shellcheck disable=SC2086 # options and sizes, split on purpose
        check "$lo $sizes, 1 to 4 threads" -t 1,2,4 $lo $sizes
    done

    # One size for the arguments, the largest of the sizes above.
    case $name in
        bmb_?gemv | bmb_?ger | bmb_?geru | bmb_?gerc) one="-m 2000 -M 1000" ;;
        bmb_?gbmv | bmb_?tbsv) one="-m 2000 -M 37" ;;
        *) case $f in */level2/*) one="-m 2000" ;; *) one="-m 512 -M 33" ;; esac ;;
    esac
    case $name in bmb_[cz]*) trans="--trans=T --trans=C" ;; *) trans=--trans=T ;; esac
    case $name in
        bmb_[sd]gemm) args="--trans=NT --trans=TN --trans=TT" ;;
        bmb_[cz]gemm) args="--trans=NT --trans=NC --trans=TN --trans=TT --trans=TC --trans=CN --trans=CT --trans=CC" ;;
        bmb_?gemv | bmb_?gbmv) args=$trans ;;
        bmb_?trmm | bmb_?trsm) args="--side=R --uplo=L $trans --diag=U" ;;
        bmb_?trmv | bmb_?trsv | bmb_?tpsv | bmb_?tbsv) args="--uplo=L $trans --diag=U" ;;
        bmb_?symm | bmb_?hemm) args="--side=R --uplo=L" ;;
        bmb_?herk | bmb_?her2k) args="--uplo=L --trans=C" ;;
        bmb_?syrk | bmb_?syr2k) args="--uplo=L --trans=T" ;;
        bmb_?symv | bmb_?spmv | bmb_?syr | bmb_?syr2 | bmb_?hemv | bmb_?her | bmb_?her2) args=--uplo=L ;;
        *) args= ;;
    esac
    for a in $args; do
        for layout in row col; do
            # shellcheck disable=SC2086
            check "$a --layout $layout $one, 1 and 4 threads" -t 1,4 "$a" --layout "$layout" $one
        done
    done
    case $name in
        bmb_?gemm)
            for layout in row col; do
                check "-K 8,300 --layout $layout -m 512 -M 33, 1 and 4 threads" \
                    -t 1,4 --layout "$layout" -m 512 -M 33 -K 8,300
                for mode in loop group strided; do
                    check "--batch-count 8 --batch-mode $mode --layout $layout -m 256 -M 33, 1 and 4 threads" \
                        -t 1,4 --layout "$layout" -m 256 -M 33 --batch-count 8 --batch-mode "$mode"
                done
            done
            ;;
    esac
    n=$((n + 1))
done

rm -f "$B/verify-sweep.err"
test "$n" -ge 32 || { echo "only $n benchmarks found in $B"; exit 1; }
test "$failed" -eq 0 || { echo "$failed of $n benchmarks computed a wrong result"; exit 1; }
echo "all $n benchmarks verified"
