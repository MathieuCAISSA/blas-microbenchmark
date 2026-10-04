#!/bin/sh
# Installs the man pages into a staging root and opens them the way a user
# would: man blas-microbenchmark, man bmb_report, and man bmb_<routine> for
# every benchmark, through its alias. Then uninstalls, and checks nothing
# is left behind.
set -e
. "${srcdir:-.}/man_helper.sh"
need man "open the installed pages"

stage=$(pwd)/$T/stage
${MAKE:-make} install DESTDIR="$stage" >"$T/install.log" 2>&1 \
    || fail "make install failed: $(cat "$T/install.log")"
mandir_staged=$stage$mandir

for page in $PAGES; do
    test -f "$stage$man1dir/$page" || fail "$page is not installed in $man1dir"
done
ok "both pages installed in $man1dir"

# MANPAGER=cat and no terminal: plain text. MANWIDTH wide enough that the
# install path, which the check below looks for, is not broken.
show() {
    MANPAGER=cat MANWIDTH=200 man -M "$mandir_staged" "$1" 2>"$T/man.err" | sed 's/.\x08//g'
}

show blas-microbenchmark | grep -q '^BLAS-MICROBENCHMARK(1)' || fail "man blas-microbenchmark: $(cat "$T/man.err")"
show bmb_report | grep -q '^BMB_REPORT(1)' || fail "man bmb_report: $(cat "$T/man.err")"
ok "man blas-microbenchmark and man bmb_report open the pages"

for r in $ROUTINES; do
    test -f "$stage$man1dir/bmb_$r.1" || fail "no alias page for bmb_$r"
    show "bmb_$r" | grep -q '^BLAS-MICROBENCHMARK(1)' || fail "man bmb_$r does not open the shared page: $(cat "$T/man.err")"
done
ok "man bmb_<routine> opens the shared page for all $(echo $ROUTINES | wc -w) benchmarks"

# The page says where the benchmarks are installed: it must be where they
# are, with this build's --prefix, written so that it can be pasted. The
# line to paste (BMB=...) is an example, which never wraps: it must be
# exact. The FILES entries wrap after a / when the path is long, so they
# are compared with the line breaks and indents taken out.
show blas-microbenchmark >"$T/page.txt"
grep -F "BMB=$pkglibexecdir" "$T/page.txt" | sed 's/^ *//' | grep -qxF "BMB=$pkglibexecdir" \
    || fail "the installed page's example does not set BMB=$pkglibexecdir on one line"
tr -d ' \n' <"$T/page.txt" >"$T/joined.txt"
for l in 1 2 3; do
    grep -qF "$(printf '%s' "$pkglibexecdir/level$l" | tr -d ' ')" "$T/joined.txt" \
        || fail "the installed page does not give $pkglibexecdir/level$l"
done
ok "the installed page gives this build's install path, $pkglibexecdir, and a line to paste it"

${MAKE:-make} uninstall DESTDIR="$stage" >"$T/uninstall.log" 2>&1 \
    || fail "make uninstall failed: $(cat "$T/uninstall.log")"
left=$(find "$stage" -type f)
test -z "$left" || fail "make uninstall left files behind: $left"
ok "make uninstall removes the pages and every alias"

rm -rf "$T"
echo "all checks passed"
