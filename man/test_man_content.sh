#!/bin/sh
# Checks what the man pages say against what the programs do: the routines
# and their dimensions, the sweeps and their limit, the JSON fields, the
# environment variables, the exit statuses, and the commands in the
# examples. A page can render perfectly and still describe last year's
# program; each check here runs the program and compares.
#
# Every benchmark run here is tiny: sizes of a few thousand elements at
# most (see AGENTS.md, "Tiny sizes only").
set -e
. "${srcdir:-.}/man_helper.sh"
S=${srcdir:-.}
PAGE=blas-microbenchmark.1

# Where the binary of a routine is, or nothing.
bin_of() {
    for l in 1 2 3; do
        if [ -x "../src/c/level$l/bmb_$1" ]; then
            echo "../src/c/level$l/bmb_$1"
            return
        fi
    done
}

# The text inside the parentheses of a JSON dimension label:
# "Matrix dim1 (M=K)" gives M=K; no label gives nothing.
label_of() {
    sed -n "s/^  \"$1\": \".*(\([^)]*\)).*/\1/p" "$2"
}

# ---- routines ---------------------------------------------------------
built=$(for f in ../src/c/level1/bmb_* ../src/c/level2/bmb_* ../src/c/level3/bmb_*; do
    n=$(basename "$f")
    case $n in *.*) continue ;; esac
    [ -f "$f" ] && [ -x "$f" ] && printf '%s\n' "${n#bmb_}"
done | sort)
test -n "$built" || fail "no benchmark found in ../src/c/level*"
listed=$(printf '%s\n' $ROUTINES | sort)
test "$built" = "$listed" || fail "ROUTINES in man/Makefile.am is not the list of benchmarks built:
built:  $(echo $built)
listed: $(echo $listed)"

names=$(section $PAGE NAME)
for r in $built; do
    printf '%s\n' "$names" | grep -q "bmb_$r[,\\ ]" || fail "bmb_$r is not in the NAME section"
done
ok "every benchmark built is named in the page and has an alias ($(echo $built | wc -w))"

# The "Routines" table: one row per level, each routine in the right one.
section $PAGE DESCRIPTION | awk -F'\t' 'NF == 2 && /^Level [123] /' >"$T/levels"
test "$(wc -l <"$T/levels")" -eq 3 || fail "the Routines table does not have one row per level"
in_table=0
while IFS='	' read -r level routines; do
    l=$(printf '%s\n' "$level" | sed 's/^Level \([123]\).*/\1/')
    for r in $routines; do
        [ -x "../src/c/level$l/bmb_$r" ] || fail "the page puts $r in level $l, where there is no bmb_$r"
        in_table=$((in_table + 1))
    done
done <"$T/levels"
test "$in_table" -eq "$(echo $built | wc -w)" || fail "the Routines table lists $in_table routines, $(echo $built | wc -w) are built"
ok "the Routines table puts each of the $in_table routines in its level"

# The table of which dimension is which, against the labels each level 2
# and 3 benchmark writes in its JSON.
unescape $PAGE | sed -n '/^Which dimension is which:/,/^\.TE/p' | grep '	' >"$T/dims"
checked=0
for r in $built; do
    b=$(bin_of "$r")
    case $b in */level1/*) continue ;; esac
    row=$(awk -F'\t' -v r="$r" '{ n = split($1, w, " "); for (i = 1; i <= n; i++) if (w[i] == r) print $2 }' "$T/dims")
    test -n "$row" || fail "$r is not in the table of dimensions"
    test "$(printf '%s\n' "$row" | wc -l)" -eq 1 || fail "$r is in the table of dimensions twice"
    want1=$(printf '%s\n' "$row" | sed -n 's/^-m is \([A-Z]\( and [A-Z]\)*\).*/\1/p' | sed 's/ and /=/g')
    want2=$(printf '%s\n' "$row" | sed -n 's/.*-M is \([A-Z]\).*/\1/p')
    "$b" -x 0 -i 1 -m 8 -o "$T/dims.json" >/dev/null
    got1=$(label_of dim1_label "$T/dims.json")
    got2=$(label_of dim2_label "$T/dims.json")
    test "$want1/$want2" = "$got1/$got2" \
        || fail "$r: the page says -m is $want1 and -M is ${want2:-absent}; the benchmark labels them $got1 and ${got2:-absent}"
    checked=$((checked + 1))
done
ok "the table of dimensions matches the labels of all $checked level 2 and 3 benchmarks"

# ---- sweeps -----------------------------------------------------------
DDOT=$(bin_of ddot)
sizes_of() {
    "$DDOT" -x 0 -i 1 -b 1 -v "$1" -o "$T/sweep.csv" >/dev/null
    grep -v '^#' "$T/sweep.csv" | sed 1d | cut -d, -f2 | tr '\n' ' ' | sed 's/ $//'
}
# Rows of the Sweeps table: "form<TAB>what<TAB>example", the example being
# either "sweep -> sizes" or a sweep that measures exactly what it lists.
unescape $PAGE | sed 's/\\(->/->/g; s/\\f[BIRP]//g' \
    | sed -n '/^\.IR sweep :/,/^\.TE/p' | grep '	' | cut -f3 >"$T/sweeps"
# The sentence about the step overshooting: "a:b:c gives x, y and z."
unescape $PAGE | sed -n 's/^\([0-9:]*\) gives \(.*\)\.$/\1 -> \2/p' | sed 's/,//g; s/ and / /' >>"$T/sweeps"
n=0
while read -r example; do
    case $example in
        *' -> '*) sweep=${example%% -> *} want=${example#* -> } ;;
        *) sweep=$example want=$(printf '%s\n' "$example" | tr ',' ' ') ;;
    esac
    got=$(sizes_of "$sweep")
    test "$got" = "$want" || fail "the page says $sweep measures $want; bmb_ddot measured $got"
    n=$((n + 1))
done <"$T/sweeps"
test "$n" -ge 5 || fail "only $n sweep examples read from the page"
ok "the $n sweep examples measure exactly the sizes the page says"

cap=$(unescape $PAGE | sed -n 's/.*at most \([0-9]*\) points.*/\1/p')
test -n "$cap" || fail "the page no longer gives the sweep's point limit"
test "$(sizes_of "1:$cap:1" | wc -w)" -eq "$cap" || fail "a sweep of $cap points was not measured in full"
if "$DDOT" -x 0 -i 1 -b 1 -v "1:$((cap + 1)):1" >/dev/null 2>&1; then
    fail "a sweep of $((cap + 1)) points was accepted; the page says at most $cap"
fi
ok "a sweep takes $cap points and refuses $((cap + 1)), as the page says"

# ---- JSON fields ------------------------------------------------------
# The fields the page names are exactly the keys bmb_print.c can write --
# not those of one run: some are left out where they do not apply, "blas"
# for a library with no version string (Netlib, NVPL, ArmPL), a machine
# field the system does not expose. The inline cache entries ({"level":
# ...}) are described by the page as caches, not field by field.
section $PAGE OUTPUT | sed -n '/^\.B json$/,/^This is what/p' | sed 1d \
    | sed -n 's/^\.BR\{0,1\} \([a-z_0-9]*\)\( .*\)\{0,1\}$/\1/p' | sort -u >"$T/documented"
grep -v '{\\"' "$S/../src/c/common/bmb_print.c" | grep -o '\\"[a-z_0-9]*\\": ' \
    | sed 's/^\\"\([a-z_0-9]*\)\\": $/\1/' | sort -u >"$T/writable"
if ! cmp -s "$T/writable" "$T/documented"; then
    echo "--- keys bmb_print.c writes"
    echo "+++ fields the page names"
    diff "$T/writable" "$T/documented" || true
    fail "the JSON fields in the page are not the ones bmb_print.c writes"
fi
# And what a run with everything on actually writes is all documented, in
# case a key ever comes from somewhere else than bmb_print.c.
"$(bin_of dgemv)" -x 0 -i 1 -m 8 -M 8 -s -l x -o "$T/all.json" >/dev/null
sed -n 's/^ *"\([a-z_0-9]*\)":.*/\1/p' "$T/all.json" | sort -u >"$T/written"
undocumented=$(comm -23 "$T/written" "$T/documented")
test -z "$undocumented" || fail "the benchmarks write JSON fields the page does not name: $(echo $undocumented)"
ok "the page names exactly the $(wc -l <"$T/writable") JSON fields bmb_print.c writes, and a real run writes no other"

# ---- environment ------------------------------------------------------
grep -o '"[A-Z_]*_NUM_THREADS"' "$S/../src/c/common/bmb_threads.c" | tr -d '"' | sort -u >"$T/read"
section $PAGE ENVIRONMENT | grep -o '[A-Z_]*_NUM_THREADS' | sort -u >"$T/named"
cmp -s "$T/read" "$T/named" || fail "the thread-count variables in ENVIRONMENT ($(echo $(cat "$T/named"))) are not the ones bmb_threads.c reads ($(echo $(cat "$T/read")))"
ok "ENVIRONMENT names the $(wc -l <"$T/read") thread-count variables the code reads"

# ---- exit statuses ----------------------------------------------------
status() {
    set +e
    "$@" >"$T/out" 2>"$T/err"
    rc=$?
    set -e
    echo $rc
}
section $PAGE "EXIT STATUS" | grep -q '^0 when' || fail "blas-microbenchmark(1) EXIT STATUS no longer starts with 0"
test "$(status "$DDOT" -x 0 -i 1 -v 8)" -eq 0 || fail "a valid run did not exit 0"
test "$(status "$DDOT" --no-such-option)" -eq 1 || fail "an invalid option did not exit 1"
test "$(status "$DDOT" -v 0)" -eq 1 || fail "an invalid size did not exit 1"
if [ -w /dev/full ]; then
    test "$(status "$DDOT" -x 0 -i 1 -v 8 -o /dev/full)" -eq 1 || fail "an unwritable -o file did not exit 1"
fi
section $PAGE "EXIT STATUS" | grep -q '^2 when' || fail "blas-microbenchmark(1) EXIT STATUS does not give 2"
test "$(BMB_VERIFY_CORRUPT=1 status "$DDOT" -x 0 -i 1 -v 8 -c)" -eq 2 || fail "a wrong result under -c did not exit 2"
ok "the benchmarks exit 0, 1 or 2 when blas-microbenchmark(1) says"

"$DDOT" -x 0 -i 1 -v 8 -o "$T/r.json" >/dev/null
test "$(status "$REPORT" "$T/r.json")" -eq 0 || fail "bmb_report on a valid result did not exit 0"
printf '{\n}\n' >"$T/bad.json"
test "$(status "$REPORT" "$T/r.json" "$T/bad.json")" -eq 1 || fail "bmb_report on a refused file did not exit 1"
test -s "$T/out" && fail "bmb_report wrote a page although it refused a file"
test "$(status "$REPORT")" -eq 2 || fail "bmb_report with no argument did not exit 2"
test "$(status "$REPORT" --no-such-option)" -eq 2 || fail "bmb_report with an unknown option did not exit 2"
ok "bmb_report exits 0, 1 or 2 when bmb_report(1) says, and writes nothing when it refuses"

# ---- examples ---------------------------------------------------------
# Every command in an example (.EX) block: the program exists, and every
# option it is given is one of the program's. An option taking a value
# skips the next word; a quoted value is two words, and the second is not
# an option either, so that holds.
options_of() {
    "$1" --help | sed -n 's/^  \(-[a-zA-Z]\), \(--[a-z0-9-]*\)\( <\)\{0,1\}.*/\1 \2 \3/p'
}
commands=0
for page in $PAGES; do
    unescape "$page" | awk '/^\.EX/ { on = 1; next } /^\.EE/ { on = 0 } on' >"$T/examples"
    while read -r line; do
        set -- $line
        while [ $# -gt 0 ]; do
            case $1 in
                bmb_report) prog=$REPORT ;;
                bmb_*) prog=$(bin_of "${1#bmb_}") ;;
                *) shift; continue ;;
            esac
            [ -n "$prog" ] || fail "$page: the example \"$line\" runs $1, which does not exist"
            options_of "$prog" >"$T/opts"
            shift
            while [ $# -gt 0 ]; do
                case $1 in
                    '#' | '>' | '|') break ;;
                    -*)
                        known=$(awk -v o="$1" '$1 == o || $2 == o { print ($3 == "<" ? "arg" : "flag") }' "$T/opts")
                        test -n "$known" || fail "$page: the example \"$line\" uses $1, which $(basename "$prog") does not take"
                        [ "$known" = arg ] && shift
                        ;;
                esac
                [ $# -gt 0 ] && shift
            done
            commands=$((commands + 1))
            break
        done
    done <"$T/examples"
done
test "$commands" -ge 10 || fail "only $commands commands found in the examples"
ok "the $commands commands in the examples run programs that exist, with options they take"

rm -rf "$T"
echo "all checks passed"
