#!/bin/sh
# Runs every benchmark with --verify (on by default), and proves each check
# can fail: with BMB_VERIFY_CORRUPT set, every benchmark's check sees its
# result altered (by a relative 1e-6 in double precision; doubled in
# single, see bmb_verify.h), and has to stop with exit status 2,
# say what is wrong, and write no results file.
#
# Without it, every benchmark has to pass at several sizes, non-square
# shapes and two thread counts -- a library can be right at one and wrong
# at another -- in both storage orders for the routines with a matrix
# (--layout row and col), each also with its leading dimensions padded
# (--ld-pad 3, #163), which the check reads through, and say in its output
# that the results were verified, and in which order. GEMM is run with a
# K of its own (-K, #215), shorter and longer than M. Every variant of
# the arguments a routine takes (--trans, --uplo, --side, --diag: #221,
# #222) is run, each combination of them: GEMM's four transposes, and
# two with C in complex; every side, triangle, transpose and diagonal of
# trmm and trsm; and so on. The defaults run in the four layouts, every
# other variant row-major and column-major padded. Without warm-up
# (-w 0): a check needs none, and it would cost 0.2 s a run.
# Tiny sizes only (see AGENTS.md).
set -e

T=verify-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

rm -rf "$T"
mkdir "$T"

# combos --opt=a,b ...: every combination of the options' values, one
# word each, --opt=a,--opt2=c, the first values first.
combos() {
    out=
    for spec in "$@"; do
        name=${spec%%=*}
        new=
        for val in $(printf '%s\n' "${spec#*=}" | tr ',' ' '); do
            if [ -z "$out" ]; then
                new="$new $name=$val"
            else
                for o in $out; do
                    new="$new $o,$name=$val"
                done
            fi
        done
        out=$new
    done
    # Combinations varying the last option slowest: put the defaults
    # (every first value) first.
    printf '%s\n' $out
}

n=0
for b in level1/*/bmb_* level2/*/bmb_* level3/*/bmb_*; do
    name=$(basename "$b")
    case $name in *.*) continue ;; esac
    [ -f "$b" ] && [ -x "$b" ] || continue
    r=${name#bmb_}
    case $b in
        level1/*) sizes="-v 7,64,1000" ;;
        *)
            case $r in
                sgemm | dgemm | cgemm | zgemm) sizes="-m 7,33,64 -M 5,64 -K 3,40" ;;
                dgemv | dger | sgemv | dsymm | dsyrk | dsyr2k | dtrmm | dtrsm \
                    | strsm | ztrsm | ssyrk | zherk | zhemm) sizes="-m 7,33,64 -M 5,64" ;;
                *) sizes="-m 7,33,64" ;;
            esac
            ;;
    esac

    case $b in
        level1/*) layouts=none ;;
        *) layouts="row col row:3 col:3" ;;
    esac
    # The routine's variants, each a word of --option=value joined by
    # commas, the defaults first.
    case $r in [cz]*) t3=N,T,C ;; *) t3=N,T ;; esac
    case $r in
        [sd]gemm) variants=$(combos --trans=NN,NT,TN,TT) ;;
        [cz]gemm) variants=$(combos --trans=NN,NT,TN,TT,CN,TC) ;;
        ?gemv) variants=$(combos --trans=$t3) ;;
        ?trmm | ?trsm) variants=$(combos --side=L,R --uplo=U,L --trans=$t3 --diag=N,U) ;;
        ?trmv | ?trsv) variants=$(combos --uplo=U,L --trans=$t3 --diag=N,U) ;;
        ?symm | ?hemm) variants=$(combos --side=L,R --uplo=U,L) ;;
        ?herk) variants=$(combos --uplo=U,L --trans=N,C) ;;
        ?syrk | ?syr2k) variants=$(combos --uplo=U,L --trans=N,T) ;;
        ?symv | ?syr | ?syr2) variants=$(combos --uplo=U,L) ;;
        *) variants=default ;;
    esac
    first=${variants%% *}
    for v in $variants; do
        case $v in default) tr= ;; *) tr=$(printf '%s\n' "$v" | tr ',' ' ') ;; esac
        # The defaults with two thread counts, in every layout; each other
        # variant, which changes the call and the check, not the threads,
        # with one, row-major and column-major padded.
        case $v in "$first") lays=$layouts threads=1,2 ;; *) lays="row col:3" threads=1 ;; esac
        for variant in $lays; do
            layout=${variant%%:*}
            pad=${variant#*:}
            if [ "$layout" = none ]; then
                lo=
            elif [ "$pad" = "$variant" ]; then
                lo="--layout $layout"
            else
                lo="--layout $layout --ld-pad $pad"
            fi

            # Right results: verified, and said so.
            if ! "$b" -x 0 -w 0 -i 1 -t $threads -c $lo $tr $sizes -o "$T/$r.json" >"$T/$r.out" 2>"$T/$r.err"; then
                fail "$name $lo $tr --verify failed on a correct library: $(cat "$T/$r.err")"
            fi
            grep -q '^# verified: ' "$T/$r.out" || fail "$name --verify does not say the results were verified"
            grep -q '^  "verified": true,$' "$T/$r.json" || fail "$name --verify: no \"verified\": true in the JSON"
            case $layout in
                none) ! grep -q '"layout"' "$T/$r.json" || fail "$name, a vector routine, records a layout" ;;
                *) grep -q "^  \"layout\": \"$layout\",\$" "$T/$r.json" || fail "$name $lo: no \"layout\": \"$layout\" in the JSON" ;;
            esac
            if [ "$pad" != "$variant" ]; then
                grep -q "^    \"ld_pad\": $pad,\$" "$T/$r.json" || fail "$name $lo: no \"ld_pad\": $pad in the JSON"
            fi

            # A wrong result: stopped, explained, nothing written.
            set +e
            BMB_VERIFY_CORRUPT=1 "$b" -x 0 -w 0 -i 1 -c $lo $tr $sizes -o "$T/$r-bad.json" >"$T/$r-bad.out" 2>"$T/$r-bad.err"
            rc=$?
            set -e
            test "$rc" -eq 2 || fail "$name $lo $tr did not exit 2 on a corrupted result (exit $rc)"
            grep -q '^Wrong result: ' "$T/$r-bad.err" || fail "$name did not say the result was wrong: $(cat "$T/$r-bad.err")"
            test -e "$T/$r-bad.json" && fail "$name wrote a results file after a wrong result"
        done
    done

    n=$((n + 1))
done
test "$n" -ge 32 || fail "only $n benchmarks found"
echo "ok   all $n benchmarks pass --verify, in both layouts, padded or not, where they have a matrix, with every --trans they take, and each one's check catches a corrupted result"

# On by default: no option, and the results are checked.
level3/double/bmb_dgemm -x 0 -i 1 -m 8 -o "$T/default.json" >"$T/default.out"
grep -q '^# verified: ' "$T/default.out" || fail "a run with no option is not verified"
grep -q '^  "verified": true,$' "$T/default.json" || fail "a run with no option has no \"verified\": true"
set +e
BMB_VERIFY_CORRUPT=1 level3/double/bmb_dgemm -x 0 -i 1 -m 8 >/dev/null 2>&1
rc=$?
set -e
test "$rc" -eq 2 || fail "a wrong result went through a run with no option (exit $rc)"
echo "ok   with no option, the results are checked, and a wrong one stops the run"

# -C: nothing claimed, and the corruption hook does nothing.
level3/double/bmb_dgemm -x 0 -i 1 -m 8 -C -o "$T/plain.json" >"$T/plain.out"
grep -q '^# verified' "$T/plain.out" && fail "a run with -C claims verified results"
grep -q '"verified"' "$T/plain.json" && fail "a run with -C has \"verified\" in its JSON"
BMB_VERIFY_CORRUPT=1 level3/double/bmb_dgemm -x 0 -i 1 -m 8 --no-verify >/dev/null \
    || fail "BMB_VERIFY_CORRUPT changed a run with --no-verify"
echo "ok   with -C or --no-verify, no claim, and BMB_VERIFY_CORRUPT has no effect"

rm -rf "$T"
echo "all checks passed"
