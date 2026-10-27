#!/bin/sh
# Checks bmb_suite (#254): every benchmark of the build tree run at tiny
# sizes through a profile file, into one directory that bmb_report then
# takes whole; what --level, --precision and --routine select; the
# shipped profiles and a routine's line winning over its level's, through
# -n; the options passed on, after the profile's, -f and -o last; and
# what it refuses before anything runs. On stand-in trees, an install's
# layout and a build tree's, made of a few scripts: a benchmark that
# fails, the next one still run, one that skips, one missing.
set -e

R=./bmb_suite
T=run-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

pass() {
    echo "ok   $*"
}

rm -rf "$T"
mkdir "$T"

# The routines bmb_suite knows, from its table: "routine level precision".
sed -n "/^ROUTINES='\$/,/^'\$/p" "$R" | tr ' ' '\n' | grep : | tr : ' ' >"$T/table"
test "$(wc -l <"$T/table")" -ge 100 || fail "only $(wc -l <"$T/table") routines read from $R"

# ---- usage ----
v=$($R --version)
test "$v" = "bmb_suite ${PACKAGE_VERSION:?PACKAGE_VERSION is not set}" \
    || fail "--version says '$v', this build is $PACKAGE_VERSION"
$R --help | grep -q '^Usage: bmb_suite' || fail "--help gives no usage"
pass "--version gives this build's version, --help the usage"

# refused MESSAGE ARG...: bmb_suite ARG... exits 2, says MESSAGE, and runs
# nothing: no directory made.
refused() {
    msg=$1
    shift
    rm -rf "$T/none"
    set +e
    $R "$@" >"$T/out" 2>"$T/err"
    rc=$?
    set -e
    test "$rc" = 2 || fail "bmb_suite $*: exit $rc, not 2: $(cat "$T/err")"
    grep -q -- "$msg" "$T/err" || fail "bmb_suite $*: no '$msg' in: $(cat "$T/err")"
    test -e "$T/none" && fail "bmb_suite $*: refused, but made $T/none"
    test -s "$T/out" && fail "bmb_suite $*: refused, but wrote: $(cat "$T/out")"
    return 0
}
refused 'Give a directory with -o'
refused 'unknown option: --nope' -o "$T/none" --nope
refused '--level needs a value' -o "$T/none" --level
refused "--level: '4' is not a level" -o "$T/none" --level 1,4
refused '--level takes a list' -o "$T/none" --level=,
refused "--precision: 'q' is not a precision" -o "$T/none" --precision q
refused "--routine: no routine dfoo" -o "$T/none" --routine ddot,dfoo
refused "--routine: '\\*' is not a routine" -o "$T/none" --routine '*'
refused 'no benchmark of .* is selected' -o "$T/none" --level 3 --routine ddot
refused 'is neither quick, full nor a file' -o "$T/none" --profile "$T/nowhere"
for w in -o -ox --output=x --out -f -fcsv --output-format=csv --peak-gflops=8 --peak-gf; do
    refused "$w: bmb_suite sets -o and -f itself" -o "$T/none" -- -i 1 "$w"
done
mkdir "$T/full"
: >"$T/full/old.json"
refused 'already holds files' -o "$T/full"
: >"$T/file"
refused 'is not a directory' -o "$T/file"
pass "refused, before anything runs: no -o, a bad option or list, a routine or profile unknown, nothing selected, -o, -f or the peak passed on, a directory holding files"

# A profile file: a line a level or a routine, comments and blank lines.
profile() {
    printf '%s\n' "$@" >"$T/profile"
}
profile 'level1 -v 64' 'levl2 -m 16'
refused "profile:2: levl2 is neither level1, level2, level3 nor a routine" -o "$T/none" --profile "$T/profile"
profile 'level1 -v 64' 'level1 -v 32'
refused "profile:2: level1 is given twice" -o "$T/none" --profile "$T/profile"
profile 'ddot -v 64 -o x.json'
refused "gives -o: bmb_suite sets -o and -f itself" -o "$T/none" --profile "$T/profile"
pass "a profile with a line that is neither a level nor a routine, one given twice, or -o: refused"

# ---- what is selected, through -n ----
# run_n ARG...: bmb_suite -n ARG...; the commands it prints, in $T/cmds.
run_n() {
    $R -n -o "$T/dry" "$@" >"$T/cmds" 2>"$T/err" || fail "bmb_suite -n $*: $(cat "$T/err")"
    test -e "$T/dry" && fail "bmb_suite -n $*: made $T/dry"
    return 0
}
# names: the routines of the commands, in their order.
names() {
    sed 's|^[^ ]*/bmb_\([a-z0-9]*\) .*|\1|' "$T/cmds" | tr '\n' ' '
}
run_n
test "$(wc -l <"$T/cmds")" = "$(wc -l <"$T/table")" ||
    fail "every benchmark: $(wc -l <"$T/cmds") commands for $(wc -l <"$T/table") routines"
test "$(names)" = "$(cut -d' ' -f1 "$T/table" | tr '\n' ' ')" || fail "not every routine in the table's order: $(names)"
run_n --level 1 --precision d
test "$(names)" = "$(awk '$2 == 1 && $3 == "d" { printf "%s ", $1 }' "$T/table")" ||
    fail "--level 1 --precision d: $(names)"
run_n --level=2,3 --precision=c,z
test "$(names)" = "$(awk '$2 >= 2 && ($3 == "c" || $3 == "z") { printf "%s ", $1 }' "$T/table")" ||
    fail "--level 2,3 --precision c,z: $(names)"
run_n --routine dgemm,ddot --precision d,s
test "$(names)" = "ddot dgemm " || fail "--routine dgemm,ddot: $(names)"
pass "every routine in the table's order; --level, --precision, --routine, each a list, together"

run_n --routine ddot,dgemm -o "$T/dry/" -- -t 1,2 -l 'a b'
grep -qx "[^ ]*/level1/double/bmb_ddot -t 1,2 -l 'a b' -f json -o $T/dry/ddot.json" "$T/cmds" ||
    fail "ddot's command, the options passed on, quoted: $(cat "$T/cmds")"
run_n --profile quick --routine ddot,dgemv,dgbmv,dgemm -- -t 2
grep -qx "[^ ]*/bmb_ddot -v 1000,100000,10000000 -t 2 -f json -o $T/dry/ddot.json" "$T/cmds" ||
    fail "ddot with the quick profile: $(cat "$T/cmds")"
grep -q "/bmb_dgemv -m 200,2000 -t 2 " "$T/cmds" || fail "dgemv with the quick profile: $(cat "$T/cmds")"
grep -q "/bmb_dgbmv -m 200,2000 -M 32 -t 2 " "$T/cmds" || fail "dgbmv with the quick profile: $(cat "$T/cmds")"
grep -q "/bmb_dgemm -m 200,1000 -t 2 " "$T/cmds" || fail "dgemm with the quick profile: $(cat "$T/cmds")"
run_n --profile=full --routine ddot,dgbmv,dgemm
grep -q "/bmb_ddot -v 256:16777216 -f json" "$T/cmds" || fail "ddot with the full profile: $(cat "$T/cmds")"
grep -q "/bmb_dgbmv -m 32:4096 -M 32 -f json" "$T/cmds" || fail "dgbmv with the full profile: $(cat "$T/cmds")"
grep -q "/bmb_dgemm -m 32:2048 -f json" "$T/cmds" || fail "dgemm with the full profile: $(cat "$T/cmds")"
pass "-n: the commands, the profile's options first, then those passed on, then -f json and -o, a word quoted where it must"

# ---- every benchmark, at tiny sizes ----
profile '# tiny, for make check' '' 'level1 -v 64' 'level2   -m 16  # a comment' 'level3 -m 8' 'dgbmv -m 16 -M 2'
$R -o "$T/results/" --profile "$T/profile" -- -x 0 -i 1 -w 0 >"$T/out" 2>"$T/err" ||
    fail "the whole suite did not run: $(cat "$T/err")"
test -s "$T/out" && fail "bmb_suite wrote to stdout: $(head -3 "$T/out")"
n=0
while read -r r l p; do
    j=$T/results/$r.json
    test -s "$j" || fail "no $j"
    test -s "$T/results/$r.log" || fail "no $T/results/$r.log"
    grep -q "^  \"routine\": \"$r\",$" "$j" || fail "$j is not $r's"
    n=$((n + 1))
done <"$T/table"
grep -q '"dim1": 64,' "$T/results/ddot.json" || fail "ddot was not run at the profile's size"
grep -q '"dim2": 2,' "$T/results/dgbmv.json" || fail "dgbmv was not run with its own line of the profile"
grep -q '"dim2": 16,' "$T/results/dgemv.json" || fail "dgemv was not run at level 2's size"
grep -q '"dim1": 8,' "$T/results/zgemm.json" || fail "zgemm was not run at level 3's size"
grep -q "^bmb_suite: ztrsm ($n/$n)\$" "$T/err" || fail "no progress line for the last one: $(tail -3 "$T/err")"
test "$(ls "$T/results" | wc -l)" = $((2 * n)) || fail "more in $T/results than a JSON and a log a routine"
pass "every benchmark ($n) run with the profile file, a JSON and a log each, a progress line each"

../report/bmb_report "$T/results" >"$T/report.html" 2>"$T/err" || fail "bmb_report refused the directory: $(cat "$T/err")"
test "$(grep -c 'class="bmb-result"' "$T/report.html")" = "$n" || fail "the report does not hold the $n results"
pass "bmb_report takes the directory: $n results"

# ---- stand-in trees ----
# A benchmark there is a script: it says its arguments and exits with the
# status its name gives (fail_<routine> files make it fail, skip_ skip).
fake() {
    mkdir -p "$(dirname "$1")"
    cat >"$1" <<'EOF'
#!/bin/sh
echo "args: $*"
case $0 in
*bmb_dgemm) echo 'no strided batch in this library' >&2; exit 77 ;;
*bmb_ddot) echo 'verification failed: expected 1, got 2' >&2; exit 2 ;;
esac
for a; do o=$a; done
echo '{}' >"$o"
EOF
    chmod +x "$1"
}

# An install: the levels' directories hold the benchmarks; bmb_suite finds
# them where it was installed to look, even beside a c/level1 that is no
# build tree, having no Makefile beside bmb_suite.
mkdir -p "$T/bin" "$T/c/level1"
sed "s|^BMB_LIBEXEC=.*|BMB_LIBEXEC='$(pwd)/$T/libexec'|" "$R" >"$T/bin/bmb_suite"
chmod +x "$T/bin/bmb_suite"
fake "$T/libexec/level1/bmb_daxpy"
fake "$T/libexec/level1/bmb_ddot"
fake "$T/libexec/level3/bmb_dgemm"
fake "$T/libexec/level3/bmb_dtrsm"
set +e
# -o last: a stand-in writes its result to its last argument.
"$T/bin/bmb_suite" -o "$T/inst" --precision d -- -i 1 >"$T/out" 2>"$T/err"
rc=$?
set -e
test "$rc" = 1 || fail "a benchmark failed, and bmb_suite exited $rc, not 1: $(cat "$T/err")"
grep -q "^bmb_suite: not in $(pwd)/$T/libexec, left out: dasum dcopy " "$T/err" ||
    fail "the routines missing from the install are not said: $(cat "$T/err")"
grep -q '^bmb_suite: ddot failed (exit 2); the end of .*/ddot.log:$' "$T/err" || fail "ddot's failure not said: $(cat "$T/err")"
grep -q '^    verification failed: expected 1, got 2$' "$T/err" || fail "ddot's log not shown: $(cat "$T/err")"
grep -q '^bmb_suite: dgemm: skipped, no strided batch in this library$' "$T/err" || fail "dgemm's skip not said: $(cat "$T/err")"
grep -q '^bmb_suite: failed: ddot$' "$T/err" || fail "the failures not listed at the end: $(cat "$T/err")"
test -s "$T/inst/daxpy.json" && test -s "$T/inst/dtrsm.json" ||
    fail "the benchmarks around a failure and a skip were not all run"
grep -q '^bmb_suite: dtrsm (4/4)$' "$T/err" || fail "not 4 run: $(cat "$T/err")"
refused_inst() {
    set +e
    "$T/bin/bmb_suite" "$@" >"$T/out" 2>"$T/err"
    rc=$?
    set -e
    test "$rc" = 2 || fail "bmb_suite $*: exit $rc, not 2"
}
refused_inst -o "$T/none" --routine ddot,dasum
grep -q -- "--routine: dasum is not in $(pwd)/$T/libexec" "$T/err" || fail "a routine asked for, missing: $(cat "$T/err")"
refused_inst -o "$T/none" --level 2
grep -q -- "no benchmark of $(pwd)/$T/libexec is selected" "$T/err" || fail "a level missing: $(cat "$T/err")"
pass "an install: its benchmarks, one missing left out or, asked for, refused; a failure said with its log, a skip, the others run, exit 1"

# A build tree: bmb_suite beside a Makefile, the benchmarks in a directory
# a precision under each level.
mkdir -p "$T/tree/run"
cp "$R" "$T/tree/run/bmb_suite"
: >"$T/tree/run/Makefile"
fake "$T/tree/c/level1/single/bmb_sdot"
fake "$T/tree/c/level2/complex16/bmb_zgemv"
"$T/tree/run/bmb_suite" -o "$T/tree-out" >"$T/out" 2>"$T/err" || fail "the build tree's: $(cat "$T/err")"
test "$(ls "$T/tree-out" | tr '\n' ' ')" = "sdot.json sdot.log zgemv.json zgemv.log " ||
    fail "a build tree's benchmarks not run: $(ls "$T/tree-out")"
pass "a build tree: a directory a precision under each level"

rm -rf "$T"
echo "all checks passed"
