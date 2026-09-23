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

for field in '"version": "' '"backend": "' '"routine": "dgemm"' '"time_s":' '"gflops":'; do
    grep -q "$field" fmt.json || fail "json carries no $field field"
done

rm -f fmt.csv fmt.json
echo "ok   output formats"
