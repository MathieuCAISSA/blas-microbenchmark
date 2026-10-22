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

./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o fmt.csv -f csv >/dev/null || fail "the csv run failed"

grep -q "^# blas-microbenchmark [0-9]" fmt.csv || fail "csv carries no version line"
grep -q "^# backend: " fmt.csv || fail "csv carries no backend line"

header=`grep -v '^#' fmt.csv | sed -n 1p`
want="thread_count,matrix_dim1_m_k,matrix_dim2_n,time_s,gflops,calls_per_sample,median_s,q1_s,q3_s"
test "$header" = "$want" || fail "csv header is '$header', expected '$want'"

rows=`grep -cv '^#' fmt.csv`
test "$rows" -eq 2 || fail "csv has $rows non-comment lines, expected 2 (header + 1 row)"

./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o fmt.json -f json >/dev/null || fail "the json run failed"

for field in '"version": "' '"backend": "' '"routine": "dgemm"' '"time_s":' '"gflops":' \
             '"dim1_label": "Matrix dim1 (M=K)"' '"dim2_label": "Matrix dim2 (N)"'; do
    grep -q "$field" fmt.json || fail "json carries no $field field"
done

# A single-dimension routine names its one dimension and leaves dim2_label
# out, the way it leaves dim2 out of every row.
../level1/double/bmb_ddot -x 0 -i 1 -v 8 -o fmt1.json -f json >/dev/null || fail "the ddot json run failed"
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
./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 --label 'say "hi"' -o fmtl.json -f json >fmtl.txt \
    || fail "the --label run failed"
grep -q '^# label: say "hi"$' fmtl.txt || fail "text output carries no label line"
grep -q '"label": "say \\"hi\\""' fmtl.json || fail "json label is missing or not escaped"

# The median and the quartiles of the samples are in every output (#161),
# after -s's mean, standard deviation, slowest and batch size, which stay
# where they were, so that a CSV read by column position keeps its
# columns: the old -s layout, then the quartiles. One sample: all three
# are that sample. Two: the median is their mean, to the last bit, and
# the quartiles a quarter of the way in from either end, interpolated as
# R and NumPy do. Three: in order, between the fastest and the slowest.
# The text gives the spread as the IQR in % of the median, last: 0 for
# one sample.
./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -s -C -o stat.csv -f csv >stat.txt || fail "the -s run failed"
header=$(grep -v '^#' stat.csv | sed -n 1p)
want="thread_count,matrix_dim1_m_k,matrix_dim2_n,time_s,gflops,mean_s,stddev_s,max_s,calls_per_sample,median_s,q1_s,q3_s"
test "$header" = "$want" || fail "-s csv header is '$header', expected '$want'"
row=$(grep -v '^#' stat.csv | sed -n 2p)
echo "$row" | awk -F, '{ exit !($4 == $10 && $4 == $11 && $4 == $12) }' \
    || fail "one sample, and the median and quartiles are not it: $row"
grep -v '^#' stat.txt | awk 'NR == 2 { exit !($NF == "0.0") }' \
    || fail "one sample, and the text gives a spread: $(grep -v '^#' stat.txt)"
./double/bmb_dgemm -x 0 -i 2 -m 8 -M 8 -s -C -o stat.csv -f csv >/dev/null || fail "the -s run failed"
row=$(grep -v '^#' stat.csv | sed -n 2p)
test "$(echo "$row" | cut -d, -f6)" = "$(echo "$row" | cut -d, -f10)" || fail "two samples, and the median is not their mean: $row"
echo "$row" | awk -F, '{ d = $8 - $4; e1 = $11 - ($4 + d / 4); e3 = $12 - ($4 + 3 * d / 4)
    exit !(e1 < 2e-9 && e1 > -2e-9 && e3 < 2e-9 && e3 > -2e-9) }' \
    || fail "two samples, and the quartiles are not a quarter of the way in: $row"
./double/bmb_dgemm -x 0 -i 3 -m 8 -M 8 -s -C -o stat.json >/dev/null || fail "the -s json run failed"
awk -F': ' '{ v = $2 + 0 } /"time_s"/ { t = v } /"q1_s"/ { a = v } /"median_s"/ { m = v } /"q3_s"/ { b = v }
    /"max_s"/ { x = v } END { exit !(t <= a && a <= m && m <= b && b <= x) }' stat.json \
    || fail "the quartiles and median are not in order between the fastest and slowest samples"
grep -q '^      "q3_s": [0-9.]*$' stat.json || fail "json has no q3_s, last in its row"

# How the run was made (#161): the command line, the program by its name,
# the warm-up, samples and batch, the compiler, the variables set, the
# affinity.
grep -q '^# command: bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o fmt.csv -f csv$' fmt.csv \
    || fail "csv has no command line: $(grep '^# command' fmt.csv)"
grep -q '^# run: 0 warm-up calls, 1 sample, batch size chosen per point, hot cache$' fmt.csv \
    || fail "csv has no run line: $(grep '^# run' fmt.csv)"
grep -q '^# compiler: [^,]*, CFLAGS ' fmt.csv || fail "csv has no compiler line"
for field in '"run": {' '"command": \["bmb_dgemm", "-x", "0", "-i", "1", "-m", "8", "-M", "8", "-o", "fmt.json", "-f", "json"\],' \
             '"warmup": 0,' '"iterations": 1,' '"cache": "hot",' '"compiler": "' '"cflags": "' '"environment": {'; do
    grep -q "$field" fmt.json || fail "json carries no $field field"
done
OMP_PROC_BIND=close BMB_MACHINE_ROOT=${srcdir:-.}/../common/fixtures/machine/container \
    ./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o run.json >run.txt 2>/dev/null || fail "the run with a variable set failed"
grep -q '^# environment: .*OMP_PROC_BIND=close' run.txt || fail "text has no OMP_PROC_BIND: $(grep '^# environment' run.txt)"
grep -q '"OMP_PROC_BIND": "close"' run.json || fail "json has no OMP_PROC_BIND"
grep -q '^# affinity: CPUs 0-3,6$' run.txt || fail "text has no affinity: $(grep '^# affinity' run.txt)"
grep -q '^    "cpus_allowed": "0-3,6",$' run.json || fail "json has no cpus_allowed"
rm -f run.json run.txt stat.txt

# The CPU frequency (#3), read from the fake sysfs trees of the machine
# probe's tests: the governor and turbo in every format, and a warning
# exactly when they can move the frequency during the run.
F=${srcdir:-.}/../common/fixtures/machine
BMB_MACHINE_ROOT=$F/x86 ./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o freq.json >freq.txt 2>freq.err \
    || fail "the run on the x86 fixture failed"
grep -q '^# frequency: governor performance, turbo off$' freq.txt || fail "no frequency line for a steady machine"
grep -q '^    "governor": "performance",$' freq.json || fail "json has no governor"
grep -q '^    "turbo": false,$' freq.json || fail "json has no \"turbo\": false"
test ! -s freq.err || fail "a steady frequency was warned about: $(cat freq.err)"

BMB_MACHINE_ROOT=$F/laptop ./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o freq.csv -f csv >freq.txt 2>freq.err \
    || fail "the run on the laptop fixture failed"
grep -q '^# frequency: governor powersave, turbo on$' freq.csv || fail "csv has no frequency line"
grep -q 'The CPU frequency can change during the run (governor powersave, turbo on)' freq.err \
    || fail "no warning for a moving frequency: $(cat freq.err)"
grep -q 'cpupower frequency-set -g performance' freq.err || fail "the warning does not say what to do"
grep -q '^Thread count' freq.txt || fail "a moving frequency stopped the run"
BMB_MACHINE_ROOT=$F/laptop ./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o freq.json >/dev/null 2>&1
grep -q '^    "turbo": true,$' freq.json || fail "json has no \"turbo\": true"

BMB_MACHINE_ROOT=$F/aarch64 ./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o freq.json >freq.txt 2>freq.err \
    || fail "the run on the aarch64 fixture failed"
if grep -q '^# frequency' freq.txt || grep -q '"governor"\|"turbo"' freq.json; then
    fail "a frequency was written for a machine that exposes none"
fi
test ! -s freq.err || fail "a warning without any frequency information: $(cat freq.err)"

# What the machine reports is written as it reads it, so a CPU name with a
# quote, a backslash or a control character in it must come out as JSON
# that still parses: escaped, \u0001 for the control character.
rm -rf odd-machine
mkdir -p odd-machine/proc
printf 'processor\t: 0\nmodel name\t: Odd\001CPU "x" \\ y\n' >odd-machine/proc/cpuinfo
BMB_MACHINE_ROOT=$PWD/odd-machine ./double/bmb_dgemm -x 0 -i 1 -m 8 -M 8 -o odd.json >/dev/null 2>&1 \
    || fail "the run on a machine with an odd CPU name failed"
grep -q '^    "cpu": "Odd\\u0001CPU \\"x\\" \\\\ y",$' odd.json \
    || fail "json cpu not escaped: $(grep '"cpu"' odd.json)"
rm -rf odd-machine odd.json

# --version on a machine whose caches cannot be read: no Caches line, the
# rest as usual.
BMB_MACHINE_ROOT=$PWD/no-such-machine ./double/bmb_dgemm -V >version.txt 2>&1 || fail "-V failed: $(cat version.txt)"
grep -q '^blas-microbenchmark [0-9]' version.txt || fail "-V printed no version: $(cat version.txt)"
if grep -q '^Caches:' version.txt; then
    fail "-V printed caches it could not read: $(cat version.txt)"
fi
BMB_MACHINE_ROOT=$F/x86 ./double/bmb_dgemm -V >version.txt 2>&1 || fail "-V failed: $(cat version.txt)"
grep -q '^Frequency: governor performance, turbo off$' version.txt \
    || fail "-V did not describe the x86 fixture's frequency: $(cat version.txt)"
rm -f version.txt

rm -f fmt.csv fmt.json fmt1.json fmtl.json fmtl.txt freq.txt freq.err freq.csv freq.json stat.csv stat.json
echo "ok   output formats"
