#!/bin/sh
# Checks bmb_suite (#254): every benchmark of the build tree run at tiny
# sizes through a profile file, into one directory that bmb_report then
# takes whole; what --level, --precision and --routine select; the
# shipped profiles and a routine's line winning over its level's, through
# -n; the options passed on, after the profile's, -f and -o last; and
# what it refuses before anything runs. On stand-in trees, an install's
# layout and a build tree's, made of a few scripts: a benchmark that
# fails, the next one still run, one that skips, one missing. With --blas
# (#255): the build tree given twice under two names, a directory and a
# label each; the forms a suite takes; a result recording another backend
# than its suite's; and what --blas refuses. With --blas flexiblas: (#256),
# on a stand-in FlexiBLAS: FLEXIBLAS set a library, a library it does not
# know refused, a suite not built against it refused; on this build, the
# library loaded now when it is built against FlexiBLAS, else refused.
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
# fake PATH [BACKEND [RECORDED]]: a stand-in benchmark, a script. It
# reads a line of its input, which would take the next suite of --blas
# were that bmb_suite's list. Its --version names BACKEND (fake by
# default; none for -; for flexiblas, flexiblas/ and the library FLEXIBLAS
# names, OPENBLAS or BLIS, else OPENBLAS, FlexiBLAS's default here, in
# lower case). Otherwise it says
# its arguments and, as its name has it, fails (ddot), skips (dgemm) or
# writes a result recording RECORDED (BACKEND by default) to its last
# argument, where bmb_suite puts -o's file.
fake() {
    mkdir -p "$(dirname "$1")"
    {
        printf '#!/bin/sh\nversion=%s\nrecorded=%s\n' "${2:-fake}" "${3:-${2:-fake}}"
        cat <<'EOF'
read -r line
if [ "$version" = flexiblas ]; then
    case ${FLEXIBLAS-} in
    OPENBLAS | BLIS) version=flexiblas/$(printf '%s' "$FLEXIBLAS" | tr '[:upper:]' '[:lower:]') ;;
    *) version=flexiblas/openblas ;;
    esac
    recorded=$version
fi
if [ "$1" = --version ]; then
    [ "$version" = - ] || echo "BLAS backend: $version (a stand-in)"
    exit 0
fi
echo "args: $*"
case $0 in
*bmb_dgemm) echo 'no strided batch in this library' >&2; exit 77 ;;
*bmb_ddot) echo 'verification failed: expected 1, got 2' >&2; exit 2 ;;
esac
for a; do o=$a; done
printf '{\n  "backend": "%s",\n}\n' "$recorded" >"$o"
EOF
    } >"$1"
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

# ---- --blas (#255) ----
# The build tree given twice, under two names, by two of the forms a suite
# takes: a directory and a label each, the backend of the build checked.
top=$(cd ../.. && pwd)
built=$(../c/level1/double/bmb_ddot --version | sed -n 's/^BLAS backend: \([^ ]*\).*/\1/p')
test -n "$built" || fail "no backend read from bmb_ddot --version"
profile 'level1 -v 64'
$R --blas one="$top" --blas two="$top/src/c" -o "$T/blas" --level 1 --precision d --profile "$T/profile" \
    -- -x 0 -i 1 -w 0 >"$T/out" 2>"$T/err" || fail "the build tree given twice: $(cat "$T/err")"
n=0
for r in $(awk '$2 == 1 && $3 == "d" { print $1 }' "$T/table"); do
    for name in one two; do
        j=$T/blas/$name/$r.json
        test -s "$j" || fail "no $j"
        grep -q "^  \"label\": \"$name\",\$" "$j" || fail "$j is not labelled $name"
        grep -q "^  \"backend\": \"$built\",\$" "$j" || fail "$j does not record $built"
    done
    n=$((n + 1))
done
grep -q "^bmb_suite: one: dasum (1/$((2 * n)))\$" "$T/err" || fail "no progress line for one's first: $(head -3 "$T/err")"
grep -q "^bmb_suite: two: drot ($((2 * n))/$((2 * n)))\$" "$T/err" || fail "no progress line for two's last: $(tail -3 "$T/err")"
test "$(ls "$T/blas" | tr '\n' ' ')" = "one two " || fail "$T/blas holds more than a directory a name: $(ls "$T/blas")"
../report/bmb_report "$T/blas" >"$T/blas.html" 2>"$T/err" || fail "bmb_report refused the two: $(cat "$T/err")"
test "$(grep -c 'class="bmb-result"' "$T/blas.html")" = $((2 * n)) || fail "the report does not hold both libraries' results"
pass "--blas, the build tree twice: a directory, a label each, every result recording $built; bmb_report takes both"

# The forms of a suite, through -n: an install prefix, the directory of
# its levels, a build tree's.
fake "$T/prefix/libexec/blas-microbenchmark/level1/bmb_daxpy"
run_n --blas p="$T/prefix" --blas l="$T/prefix/libexec/blas-microbenchmark" --blas t="$top/src/c" --routine daxpy -- -i 1
grep -qx "$T/prefix/libexec/blas-microbenchmark/level1/bmb_daxpy -i 1 --label p -f json -o $T/dry/p/daxpy.json" "$T/cmds" ||
    fail "an install prefix: $(cat "$T/cmds")"
grep -qx "$T/prefix/libexec/blas-microbenchmark/level1/bmb_daxpy -i 1 --label l -f json -o $T/dry/l/daxpy.json" "$T/cmds" ||
    fail "the directory of its levels: $(cat "$T/cmds")"
grep -qx "$top/src/c/level1/double/bmb_daxpy -i 1 --label t -f json -o $T/dry/t/daxpy.json" "$T/cmds" ||
    fail "a build tree's levels: $(cat "$T/cmds")"
pass "--blas: an install prefix, the directory of its levels, a build tree; the label after the options passed on"

# A result recording another backend than its suite's --version is a
# failure, and the others still run.
fake "$T/mixed/level1/bmb_daxpy" fake other
fake "$T/mixed/level1/bmb_dcopy"
set +e
$R --blas m="$T/mixed" --blas p="$T/prefix" -o "$T/mixed-out" --precision d >"$T/out" 2>"$T/err"
rc=$?
set -e
test "$rc" = 1 || fail "a result of another backend: exit $rc, not 1: $(cat "$T/err")"
grep -q "^bmb_suite: m: daxpy records the backend other, not fake, which $T/mixed was built for\$" "$T/err" ||
    fail "a result of another backend not said: $(cat "$T/err")"
grep -q '^bmb_suite: failed: m/daxpy$' "$T/err" || fail "m/daxpy not listed as failed: $(cat "$T/err")"
test -s "$T/mixed-out/m/dcopy.json" || fail "the next benchmark was not run"
test -s "$T/mixed-out/p/daxpy.json" || fail "the next suite was not run: $(cat "$T/err")"
pass "--blas: a result recording another backend than its suite's is a failure"

fake "$T/mute/level1/bmb_daxpy" -
refused 'the backend of .* cannot be read' -o "$T/none" --blas m="$T/mute"
refused "--blas takes <name>=<suite>" -o "$T/none" --blas openblas
refused "--blas takes <name>=<suite>" -o "$T/none" --blas=openblas=
refused "'' is not a name" -o "$T/none" --blas ="$top"
refused "'a b' is not a name" -o "$T/none" --blas "a b=$top"
refused "'.a' is not a name" -o "$T/none" --blas .a="$top"
refused "one is given twice" -o "$T/none" --blas one="$top" --blas one="$top/src/c"
refused "no blas-microbenchmark in $T" -o "$T/none" --blas x="$T"
for w in -l -lx --label=x --lab; do
    refused "$w: with --blas, bmb_suite labels" -o "$T/none" --blas one="$top" -- "$w" x
done
pass "--blas refused: no name or suite, a name that is no directory name or given twice, a path with no suite, a backend unread, a label passed on"

# ---- --blas flexiblas: (#256) ----
# On a stand-in install built against a stand-in FlexiBLAS, which knows
# OPENBLAS and BLIS: FLEXIBLAS set a library, a directory and a label
# each, each result recording the library loaded.
mkdir -p "$T/fbin"
sed "s|^BMB_LIBEXEC=.*|BMB_LIBEXEC='$(pwd)/$T/flibexec'|" "$R" >"$T/fbin/bmb_suite"
chmod +x "$T/fbin/bmb_suite"
fake "$T/flibexec/level1/bmb_daxpy" flexiblas
fake "$T/flibexec/level1/bmb_dcopy" flexiblas
"$T/fbin/bmb_suite" --blas flexiblas:OPENBLAS,BLIS -o "$T/flex" -- -i 1 >"$T/out" 2>"$T/err" ||
    fail "--blas flexiblas:OPENBLAS,BLIS: $(cat "$T/err")"
for name in OPENBLAS BLIS; do
    for r in daxpy dcopy; do
        grep -qx "  \"backend\": \"flexiblas/$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')\"," "$T/flex/$name/$r.json" ||
            fail "$T/flex/$name/$r.json did not load $name"
        grep -q -- "-i 1 --label $name -f json" "$T/flex/$name/$r.log" || fail "$name's $r: $(cat "$T/flex/$name/$r.log")"
    done
done
grep -q '^bmb_suite: BLIS: dcopy (4/4)$' "$T/err" || fail "no progress line for BLIS's last: $(cat "$T/err")"
pass "--blas flexiblas:OPENBLAS,BLIS: FLEXIBLAS set a library, a directory and a label each"

"$T/fbin/bmb_suite" -n --blas flexiblas:BLIS --blas p="$T/prefix" --routine daxpy -o "$T/dry" >"$T/cmds" 2>"$T/err" ||
    fail "flexiblas: and a suite, -n: $(cat "$T/err")"
grep -qx "FLEXIBLAS=BLIS $(pwd)/$T/flibexec/level1/bmb_daxpy --label BLIS -f json -o $T/dry/BLIS/daxpy.json" "$T/cmds" ||
    fail "the FlexiBLAS library's command: $(cat "$T/cmds")"
grep -qx "$T/prefix/libexec/blas-microbenchmark/level1/bmb_daxpy --label p -f json -o $T/dry/p/daxpy.json" "$T/cmds" ||
    fail "the suite's command, beside: $(cat "$T/cmds")"
pass "--blas flexiblas: beside --blas <name>=<suite>; -n gives FLEXIBLAS"

# refused_f MESSAGE ARG...: the stand-in FlexiBLAS install refuses ARG...
refused_f() {
    msg=$1
    shift
    set +e
    "$T/fbin/bmb_suite" -o "$T/none" "$@" >"$T/out" 2>"$T/err"
    rc=$?
    set -e
    test "$rc" = 2 || fail "bmb_suite $*: exit $rc, not 2: $(cat "$T/err")"
    grep -q -- "$msg" "$T/err" || fail "bmb_suite $*: no '$msg' in: $(cat "$T/err")"
    test -e "$T/none" && fail "bmb_suite $*: refused, but made $T/none"
    return 0
}
refused_f "FLEXIBLAS=NETLIB loads flexiblas/openblas; 'flexiblas list' gives the names" --blas flexiblas:BLIS,NETLIB
refused_f "--blas flexiblas: takes the names" --blas flexiblas:
refused_f "--blas flexiblas: takes the names" --blas flexiblas:,
refused_f "BLIS is given twice" --blas flexiblas:BLIS,OPENBLAS,BLIS
refused_f "BLIS is given twice" --blas flexiblas:BLIS --blas BLIS="$T/prefix"
refused_f "'a/b' is not a name" --blas flexiblas:a/b
set +e
"$T/bin/bmb_suite" -o "$T/none" --blas flexiblas:BLIS >"$T/out" 2>"$T/err"
rc=$?
set -e
test "$rc" = 2 && grep -q "is built against fake, not FlexiBLAS" "$T/err" ||
    fail "a suite not built against FlexiBLAS: exit $rc, $(cat "$T/err")"
pass "--blas flexiblas: refused: a library FlexiBLAS does not know, no name, one given twice, a suite not built against it"

# The build tree's own: in the CI job that builds against FlexiBLAS, the
# library loaded now, through bmb_suite; anywhere else, refused.
case $built in
flexiblas/*)
    lib=$(printf '%s' "${built#flexiblas/}" | tr '[:lower:]' '[:upper:]')
    $R --blas flexiblas:"$lib" -o "$T/own" --routine ddot -- -x 0 -i 1 -w 0 -v 64 >"$T/out" 2>"$T/err" ||
        fail "--blas flexiblas:$lib on the build tree: $(cat "$T/err")"
    grep -qx "  \"backend\": \"$built\"," "$T/own/$lib/ddot.json" || fail "the build tree's $lib: not $built"
    pass "--blas flexiblas:$lib, on this build against FlexiBLAS"
    ;;
*)
    refused "is built against $built, not FlexiBLAS" -o "$T/none" --blas flexiblas:OPENBLAS
    pass "--blas flexiblas: refused on this build, against $built"
    ;;
esac

rm -rf "$T"
echo "all checks passed"
