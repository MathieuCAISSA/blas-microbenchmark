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

# -s adds the median of the samples, last, so that a CSV read by column
# position keeps its other columns where they were. One sample: it is
# that sample. Two: their mean, to the last bit. Three: one of them,
# between the fastest and the slowest.
./bmb_dgemm -x 0 -i 1 -m 8 -M 8 -s -C -o stat.csv -f csv >/dev/null || fail "the -s run failed"
header=$(grep -v '^#' stat.csv | sed -n 1p)
want="thread_count,matrix_dim1_m_k,matrix_dim2_n,time_s,gflops,mean_s,stddev_s,max_s,calls_per_sample,median_s"
test "$header" = "$want" || fail "-s csv header is '$header', expected '$want'"
row=$(grep -v '^#' stat.csv | sed -n 2p)
test "$(echo "$row" | cut -d, -f4)" = "$(echo "$row" | cut -d, -f10)" || fail "one sample, and the median is not it: $row"
./bmb_dgemm -x 0 -i 2 -m 8 -M 8 -s -C -o stat.csv -f csv >/dev/null || fail "the -s run failed"
row=$(grep -v '^#' stat.csv | sed -n 2p)
test "$(echo "$row" | cut -d, -f6)" = "$(echo "$row" | cut -d, -f10)" || fail "two samples, and the median is not their mean: $row"
./bmb_dgemm -x 0 -i 3 -m 8 -M 8 -s -C -o stat.json >/dev/null || fail "the -s json run failed"
awk -F': ' '/"time_s"/ { t = $2 + 0 } /"max_s"/ { x = $2 + 0 } /"median_s"/ { m = $2 + 0 }
    END { exit !(m >= t && m <= x) }' stat.json || fail "the median is not between the fastest and slowest samples"
grep -q '^      "median_s": [0-9.]*$' stat.json || fail "json has no median_s, last in its row"

# The CPU frequency (#3), read from the fake sysfs trees of the machine
# probe's tests: the governor and turbo in every format, and a warning
# exactly when they can move the frequency during the run.
F=${srcdir:-.}/../common/fixtures/machine
BMB_MACHINE_ROOT=$F/x86 ./bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o freq.json >freq.txt 2>freq.err \
    || fail "the run on the x86 fixture failed"
grep -q '^# frequency: governor performance, turbo off$' freq.txt || fail "no frequency line for a steady machine"
grep -q '^    "governor": "performance",$' freq.json || fail "json has no governor"
grep -q '^    "turbo": false,$' freq.json || fail "json has no \"turbo\": false"
test ! -s freq.err || fail "a steady frequency was warned about: $(cat freq.err)"

BMB_MACHINE_ROOT=$F/laptop ./bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o freq.csv -f csv >freq.txt 2>freq.err \
    || fail "the run on the laptop fixture failed"
grep -q '^# frequency: governor powersave, turbo on$' freq.csv || fail "csv has no frequency line"
grep -q 'The CPU frequency can change during the run (governor powersave, turbo on)' freq.err \
    || fail "no warning for a moving frequency: $(cat freq.err)"
grep -q 'cpupower frequency-set -g performance' freq.err || fail "the warning does not say what to do"
grep -q '^Thread count' freq.txt || fail "a moving frequency stopped the run"
BMB_MACHINE_ROOT=$F/laptop ./bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o freq.json >/dev/null 2>&1
grep -q '^    "turbo": true,$' freq.json || fail "json has no \"turbo\": true"

BMB_MACHINE_ROOT=$F/aarch64 ./bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o freq.json >freq.txt 2>freq.err \
    || fail "the run on the aarch64 fixture failed"
if grep -q '^# frequency' freq.txt || grep -q '"governor"\|"turbo"' freq.json; then
    fail "a frequency was written for a machine that exposes none"
fi
test ! -s freq.err || fail "a warning without any frequency information: $(cat freq.err)"

rm -f fmt.csv fmt.json fmt1.json fmtl.json fmtl.txt freq.txt freq.err freq.csv freq.json stat.csv stat.json
echo "ok   output formats"
