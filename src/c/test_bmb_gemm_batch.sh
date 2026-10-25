#!/bin/sh
# Batched GEMM (#159): each of sgemm, dgemm, cgemm and zgemm, a batch of
# matrices in each --batch-mode the library has (loop everywhere, group
# where config.h says cblas_?gemm_batch is there, strided where
# cblas_?gemm_batch_strided is), passes --verify, every matrix of it, in
# both layouts, transposed or not, and its check catches a corrupted
# result; a mode the library has not is skipped, exit 77 and a message,
# nothing measured. The count and the mode are in every output; a routine
# without a batched form refuses the options; a batch too large for
# size_t is refused before anything is allocated. Tiny sizes only.
set -e

T=gemm-batch-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

config=${top_builddir:?top_builddir is not set}/config.h
has() {
    grep -q "^#define $1 1\$" "$config"
}

rm -rf "$T"
mkdir "$T"

modes=loop
missing=
if has HAVE_CBLAS_GEMM_BATCH; then modes="$modes group"; else missing="$missing group"; fi
if has HAVE_CBLAS_GEMM_BATCH_STRIDED; then modes="$modes strided"; else missing="$missing strided"; fi

for b in level3/single/bmb_sgemm level3/double/bmb_dgemm level3/complex/bmb_cgemm level3/complex16/bmb_zgemm; do
    r=${b##*/bmb_}
    for mode in $modes; do
        for args in "--layout row --trans NN" "--layout col --trans TN -p 3"; do
            # shellcheck disable=SC2086 # the options, split on purpose
            "$b" -x 0 -w 0 -i 1 -m 7,20 -M 5 -K 3,9 --batch-count 4 --batch-mode "$mode" $args \
                -o "$T/$r.json" >"$T/$r.out" 2>"$T/$r.err" ||
                fail "$r --batch-mode $mode $args failed: $(cat "$T/$r.err")"
            grep -q '^# verified: ' "$T/$r.out" || fail "$r --batch-mode $mode $args is not verified"
        done
        grep -q "^# arguments: trans TN, batch_count 4, batch_mode $mode\$" "$T/$r.out" ||
            fail "$r --batch-mode $mode: the table does not say the batch: $(grep '^# arg' "$T/$r.out")"
        grep -q "\"batch_count\": 4, \"batch_mode\": \"$mode\"" "$T/$r.json" ||
            fail "$r --batch-mode $mode: the json does not say the batch"
        set +e
        BMB_VERIFY_CORRUPT=1 "$b" -x 0 -w 0 -i 1 -m 7 --batch-count 4 --batch-mode "$mode" >/dev/null 2>"$T/$r.err"
        rc=$?
        set -e
        test "$rc" -eq 2 || fail "$r --batch-mode $mode: a corrupted result exited $rc, not 2"
        grep -q 'of matrix 0 of the batch' "$T/$r.err" || fail "$r --batch-mode $mode: $(cat "$T/$r.err")"
    done
    for mode in $missing; do
        set +e
        "$b" -x 0 -w 0 -i 1 -m 7 --batch-count 4 --batch-mode "$mode" >"$T/$r.out" 2>"$T/$r.err"
        rc=$?
        set -e
        test "$rc" -eq 77 || fail "$r --batch-mode $mode, which this library has not: exit $rc, not 77"
        grep -q "has no cblas_?gemm_batch.* call; nothing measured" "$T/$r.err" ||
            fail "$r --batch-mode $mode: no message saying why: $(cat "$T/$r.err")"
        if grep -q '^[0-9]' "$T/$r.out"; then
            fail "$r --batch-mode $mode measured something"
        fi
    done
done
echo "ok   the four GEMMs batched, in$(printf ' %s' $modes), every matrix verified; skipped:${missing:- none}"

# One matrix in a loop is the plain call: the defaults, recorded.
level3/double/bmb_dgemm -x 0 -w 0 -i 1 -m 7 >"$T/one.out" || fail "dgemm failed"
grep -q '^# arguments: trans NN, batch_count 1, batch_mode loop$' "$T/one.out" ||
    fail "dgemm without a batch does not record one matrix in a loop: $(grep '^# arg' "$T/one.out")"
echo "ok   without the options, one matrix in a loop, recorded"

# A routine without a batched form refuses the options, before anything
# is measured.
for opt in "--batch-count 2" "--batch-mode loop"; do
    # shellcheck disable=SC2086 # the option and its value
    if level3/double/bmb_dsyrk -x 0 -w 0 -i 1 -m 7 $opt >"$T/out" 2>"$T/err"; then
        fail "dsyrk took $opt"
    fi
    grep -q 'dsyrk has no batched form' "$T/err" || fail "dsyrk $opt: $(cat "$T/err")"
done
echo "ok   a routine without a batched form refuses --batch-count and --batch-mode"

# A batch whose matrices together would wrap size_t: refused, nothing
# allocated. A, M x K at the parser's ceiling, fits size_t alone; two of
# them do not.
if level3/double/bmb_dgemm -x 0 -w 0 -i 1 -C -m 1518500249 -M 1 -K 1518500249 --batch-count 2 >"$T/out" 2>"$T/err"; then
    fail "a batch too large for size_t was taken"
fi
grep -q 'more bytes than size_t can count' "$T/err" || fail "a batch too large for size_t: $(cat "$T/err")"
echo "ok   a batch too large for size_t is refused"

rm -rf "$T"
echo "all checks passed"
