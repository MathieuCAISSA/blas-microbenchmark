#!/bin/sh
# Every list of the routines is the one src/c/routines.txt gives (#199):
# the levels' Makefile.am, man/Makefile.am, the man page's NAME and its two
# tables, the README's, the report's LEVEL and the ReFrame tests' ROUTINES, as
# src/c/routines.sh writes them. On a copy of those files, a routine
# added to the table alone makes each list it belongs in stale, until
# routines.sh writes them again; a malformed line, a routine listed twice
# and a missing marker each stop it with a message.
set -e

top=${srcdir:-.}
T=routines-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

sh "$top/src/c/routines.sh" --check "$top" >"$T.out" 2>&1 ||
    fail "a list is not what the table gives: $(cat "$T.out")"
rm -f "$T.out"
echo "ok   every list is the table's"

files='src/c/routines.txt src/c/routines.sh src/c/level1/Makefile.am
src/c/level2/Makefile.am src/c/level3/Makefile.am man/Makefile.am
man/blas-microbenchmark.1.in README.md src/report/report.html
packaging/reframe/blas_microbenchmark.py'
fresh() {
    rm -rf "$T"
    # shellcheck disable=SC2086 # one word per file, none with a space
    for f in $files; do
        mkdir -p "$T/$(dirname "$f")"
        # Not cp, which keeps the read-only mode of distcheck's sources.
        cat "$top/$f" >"$T/$f"
    done
}

fresh
# zfake, of a family fake: a routine the table will never have, which no
# source needs here.
echo 'zfake      3      z          mn     fake' >>"$T/src/c/routines.txt"
if sh "$T/src/c/routines.sh" --check "$T" >"$T/out" 2>&1; then
    fail "--check passed with zfake in the table alone"
fi
n=$(grep -c 'is not the table' "$T/out")
# Every list but level 1's and level 2's Makefile.am, the man page's three
# counted apart.
test "$n" = 8 || fail "zfake in the table alone: $n lists stale, not 8: $(cat "$T/out")"
grep -q '^+	complex16/bmb_zfake$' "$T/out" ||
    fail "the stale level 3 Makefile.am's difference does not add zfake: $(cat "$T/out")"
echo "ok   a routine in the table alone: the 8 lists it belongs in stale"

sh "$T/src/c/routines.sh" "$T" >/dev/null || fail "routines.sh could not write the lists"
sh "$T/src/c/routines.sh" --check "$T" >"$T/out" 2>&1 ||
    fail "--check failed on what routines.sh wrote: $(cat "$T/out")"
grep -q "zfake: 3$" "$T/src/report/report.html" || fail "zfake not in the report's LEVEL"
grep -q "'zfake': 3,$" "$T/packaging/reframe/blas_microbenchmark.py" || fail "zfake not in ReFrame's ROUTINES"
grep -q '^complex16_bmb_zfake_SOURCES = bmb_fake.c$' "$T/src/c/level3/Makefile.am" ||
    fail "zfake is not built from its family's source in level 3's Makefile.am"
grep -q '^complex16_bmb_zfake_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z$' "$T/src/c/level3/Makefile.am" ||
    fail "zfake's source is not compiled for double complex in level 3's Makefile.am"
grep -q '`ztrsm` `zfake` |$' "$T/README.md" || fail "zfake not in the README's complex column"
grep -q 'zfake	\\-m is M, \\-M is N$' "$T/man/blas-microbenchmark.1.in" ||
    fail "zfake not in the man page's table of dimensions"
grep -E -q '(^| )bmb_zfake \\- microbenchmarks for BLAS routines$' "$T/man/blas-microbenchmark.1.in" ||
    fail "zfake not last in the man page's NAME"
echo "ok   routines.sh writes them, and --check then passes"

# refused LINE MESSAGE: the table with LINE added stops routines.sh with
# MESSAGE, and changes nothing.
refused() {
    fresh
    echo "$1" >>"$T/src/c/routines.txt"
    if sh "$T/src/c/routines.sh" "$T" >"$T/out" 2>&1; then
        fail "a table with '$1' was taken"
    fi
    grep -q "$2" "$T/out" || fail "'$1': no '$2' in: $(cat "$T/out")"
    cmp -s "$top/README.md" "$T/README.md" || fail "'$1' was refused after changing the README"
}
refused 'zaxpy 1 z v' 'not "routine level precision sizes family"'
refused 'zaxpy 4 z v axpy' 'not "routine level precision sizes family"'
refused 'zaxpy 1 q v axpy' 'not "routine level precision sizes family"'
refused 'zaxpy 1 z mk axpy' 'not "routine level precision sizes family"'
refused 'zaxpy 1 z v Axpy' 'not "routine level precision sizes family"'
refused 'zaxpy 1 z v -' 'not "routine level precision sizes family"'
refused 'ddot 1 d v dot' 'ddot listed twice'
echo "ok   a malformed line, a routine listed twice: refused"

fresh
grep -v 'END routines (reframe)' "$top/packaging/reframe/blas_microbenchmark.py" \
    >"$T/packaging/reframe/blas_microbenchmark.py"
if sh "$T/src/c/routines.sh" --check "$T" >"$T/out" 2>&1; then
    fail "--check passed without ReFrame's END marker"
fi
grep -q 'needs one BEGIN and one END routines (reframe) line' "$T/out" ||
    fail "a missing marker, and no message: $(cat "$T/out")"
echo "ok   a missing marker: refused"

rm -rf "$T"
echo "all checks passed"
