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

# The machine and the date are recorded in every format (#1, decision 4).
for line in '^# cpu: ' '^# os: ' '^# date: '; do
    grep -q "$line" fmt.csv || fail "csv carries no '$line' line"
done
for field in '"machine": {' '"date": "' '"logical_cpus": '; do
    grep -q "$field" fmt.json || fail "json carries no $field field"
done

# No --label, no label anywhere: the field is omitted rather than empty.
if grep -q '^# label:' fmt.csv || grep -q '"label"' fmt.json; then
    fail "a label was written although none was given"
fi

# With one, it shows up in every format -- escaped in JSON, since it is
# whatever the user typed.
./bmb_dgemm -x 0 -i 1 -m 8 -M 8 --label 'say "hi"' -o fmtl.json -f json >fmtl.txt \
    || fail "the --label run failed"
grep -q '^# label: say "hi"$' fmtl.txt || fail "text output carries no label line"
grep -q '"label": "say \\"hi\\""' fmtl.json || fail "json label is missing or not escaped"

rm -f fmt.csv fmt.json fmt1.json fmtl.json fmtl.txt
echo "ok   output formats"
