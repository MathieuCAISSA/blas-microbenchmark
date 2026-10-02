#!/bin/sh
# The --output files are not covered by the per-routine smoke tests, which
# only look at stdout. dgemm is the routine used here because its
# "Matrix dim1 (M=K)" label is the one that exercises the CSV column-name
# rewriting hardest.
set -e

fail() {
    echo "FAIL: $*"
    exit 1
}

./bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o fmt.csv -f csv >/dev/null || fail "the csv run failed"

grep -q "^# blas-microbenchmark [0-9]" fmt.csv || fail "csv carries no version line"
grep -q "^# backend: " fmt.csv || fail "csv carries no backend line"

header=`grep -v '^#' fmt.csv | sed -n 1p`
want="thread_count,matrix_dim1_m_k,matrix_dim2_n,time_s,gflops"
test "$header" = "$want" || fail "csv header is '$header', expected '$want'"

rows=`grep -cv '^#' fmt.csv`
test "$rows" -eq 2 || fail "csv has $rows non-comment lines, expected 2 (header + 1 row)"

./bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o fmt.json -f json >/dev/null || fail "the json run failed"

for field in '"version": "' '"backend": "' '"routine": "dgemm"' '"time_s":' '"gflops":' \
             '"dim1_label": "Matrix dim1 (M=K)"' '"dim2_label": "Matrix dim2 (N)"'; do
    grep -q "$field" fmt.json || fail "json carries no $field field"
done

# A single-dimension routine names its one dimension and leaves dim2_label
# out, the way it leaves dim2 out of every row.
../level1/bmb_ddot -x 0 -i 1 -v 8 -o fmt1.json -f json >/dev/null || fail "the ddot json run failed"
grep -q '"dim1_label": "Vector size"' fmt1.json || fail "ddot json has no dim1_label"
if grep -q '"dim2_label"' fmt1.json; then
    fail "ddot json carries a dim2_label for a routine with one dimension"
fi

rm -f fmt.csv fmt.json fmt1.json
echo "ok   output formats"
