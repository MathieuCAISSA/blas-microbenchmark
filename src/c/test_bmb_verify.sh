#!/bin/sh
# Runs every benchmark with --verify (on by default), and proves each check
# can fail: with BMB_VERIFY_CORRUPT set, every benchmark's check sees its
# result altered by a relative 1e-6, and has to stop with exit status 2,
# say what is wrong, and write no results file.
#
# Without it, every benchmark has to pass at several sizes, non-square
# shapes and two thread counts -- a library can be right at one and wrong
# at another -- in both storage orders for the routines with a matrix
# (--layout row and col), and say in its output that the results were
# verified, and in which order.
# Tiny sizes only (see AGENTS.md).
set -e

T=verify-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

rm -rf "$T"
mkdir "$T"

n=0
for b in level1/bmb_* level2/bmb_* level3/bmb_*; do
    name=$(basename "$b")
    case $name in *.*) continue ;; esac
    [ -f "$b" ] && [ -x "$b" ] || continue
    r=${name#bmb_}
    case $b in
        level1/*) sizes="-v 7,64,1000" ;;
        *)
            case $r in
                dgemv | dger | dgemm | dsymm | dsyrk | dsyr2k | dtrmm | dtrsm) sizes="-m 7,33,64 -M 5,64" ;;
                *) sizes="-m 7,33,64" ;;
            esac
            ;;
    esac

    case $b in
        level1/*) layouts=none ;;
        *) layouts="row col" ;;
    esac
    for layout in $layouts; do
        if [ "$layout" = none ]; then
            lo=
        else
            lo="--layout $layout"
        fi

        # Right results: verified, and said so.
        if ! "$b" -x 0 -i 1 -t 1,2 -c $lo $sizes -o "$T/$r.json" >"$T/$r.out" 2>"$T/$r.err"; then
            fail "$name $lo --verify failed on a correct library: $(cat "$T/$r.err")"
        fi
        grep -q '^# verified: ' "$T/$r.out" || fail "$name --verify does not say the results were verified"
        grep -q '^  "verified": true,$' "$T/$r.json" || fail "$name --verify: no \"verified\": true in the JSON"
        case $layout in
            none) ! grep -q '"layout"' "$T/$r.json" || fail "$name, a vector routine, records a layout" ;;
            *) grep -q "^  \"layout\": \"$layout\",\$" "$T/$r.json" || fail "$name $lo: no \"layout\": \"$layout\" in the JSON" ;;
        esac

        # A wrong result: stopped, explained, nothing written.
        set +e
        BMB_VERIFY_CORRUPT=1 "$b" -x 0 -i 1 -c $lo $sizes -o "$T/$r-bad.json" >"$T/$r-bad.out" 2>"$T/$r-bad.err"
        rc=$?
        set -e
        test "$rc" -eq 2 || fail "$name $lo did not exit 2 on a corrupted result (exit $rc)"
        grep -q '^Wrong result: ' "$T/$r-bad.err" || fail "$name did not say the result was wrong: $(cat "$T/$r-bad.err")"
        test -e "$T/$r-bad.json" && fail "$name wrote a results file after a wrong result"
    done

    n=$((n + 1))
done
test "$n" -ge 20 || fail "only $n benchmarks found"
echo "ok   all $n benchmarks pass --verify, in both layouts where they have a matrix, and each one's check catches a corrupted result"

# On by default: no option, and the results are checked.
level3/bmb_dgemm -x 0 -i 1 -m 8 -o "$T/default.json" >"$T/default.out"
grep -q '^# verified: ' "$T/default.out" || fail "a run with no option is not verified"
grep -q '^  "verified": true,$' "$T/default.json" || fail "a run with no option has no \"verified\": true"
set +e
BMB_VERIFY_CORRUPT=1 level3/bmb_dgemm -x 0 -i 1 -m 8 >/dev/null 2>&1
rc=$?
set -e
test "$rc" -eq 2 || fail "a wrong result went through a run with no option (exit $rc)"
echo "ok   with no option, the results are checked, and a wrong one stops the run"

# -C: nothing claimed, and the corruption hook does nothing.
level3/bmb_dgemm -x 0 -i 1 -m 8 -C -o "$T/plain.json" >"$T/plain.out"
grep -q '^# verified' "$T/plain.out" && fail "a run with -C claims verified results"
grep -q '"verified"' "$T/plain.json" && fail "a run with -C has \"verified\" in its JSON"
BMB_VERIFY_CORRUPT=1 level3/bmb_dgemm -x 0 -i 1 -m 8 --no-verify >/dev/null \
    || fail "BMB_VERIFY_CORRUPT changed a run with --no-verify"
echo "ok   with -C or --no-verify, no claim, and BMB_VERIFY_CORRUPT has no effect"

rm -rf "$T"
echo "all checks passed"
