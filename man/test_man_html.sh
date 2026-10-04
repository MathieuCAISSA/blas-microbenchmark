#!/bin/sh
# Checks the documentation site html.sh builds from the man pages: every
# page and the index are there, every link on the site leads somewhere --
# to a file and an anchor that exist, or to https -- nothing is loaded
# from elsewhere, the pages link to each other, and each page holds what
# its man page does (every option of --help, the version).
set -e
. "${srcdir:-.}/man_helper.sh"
need mandoc "build the site"

site=$T/site
srcdir=${srcdir:-.} "${SHELL:-/bin/sh}" "${srcdir:-.}/html.sh" "$site" "$PACKAGE_VERSION" $PAGES \
    >"$T/build.log" 2>&1 || fail "html.sh failed: $(cat "$T/build.log")"

# A site with empty pages must not pass for a site: without mandoc, or
# when it fails, html.sh fails too. (It once wrote empty pages and said
# nothing, the failure hidden in a pipe.)
for bad in "$T/no-such-mandoc" false; do
    if MANDOC=$bad srcdir=${srcdir:-.} "${SHELL:-/bin/sh}" "${srcdir:-.}/html.sh" "$T/bad" "$PACKAGE_VERSION" $PAGES \
        >/dev/null 2>&1; then
        fail "html.sh succeeded with MANDOC=$bad"
    fi
done
ok "html.sh fails when mandoc is missing or fails"

for f in index.html mandoc.css blas-microbenchmark.html bmb_report.html; do
    test -s "$site/$f" || fail "the site has no $f"
done
ok "the site has the index, the stylesheet and both pages"

for f in "$site"/*.html; do
    grep -q "blas-microbenchmark $PACKAGE_VERSION" "$f" || fail "$(basename "$f") does not give the version, $PACKAGE_VERSION"
done
ok "every page gives the version, $PACKAGE_VERSION"

# Links: href="..." of every page. A relative one names a file of the site
# and, after #, an id in it; an absolute one must be https.
n=0
for f in "$site"/*.html; do
    for href in $(grep -o 'href="[^"]*"' "$f" | sed 's/^href="//; s/"$//'); do
        case $href in
            https://*) ;;
            http://* | //*) fail "$(basename "$f") links to $href, which is not https" ;;
            *)
                file=${href%%#*}
                frag=
                case $href in *'#'*) frag=${href#*#} ;; esac
                target=$f
                if [ -n "$file" ]; then
                    target=$site/$file
                    test -f "$target" || fail "$(basename "$f") links to $href, and the site has no $file"
                fi
                if [ -n "$frag" ]; then
                    grep -q "id=\"$frag\"" "$target" || fail "$(basename "$f") links to $href, and $(basename "$target") has no id \"$frag\""
                fi
                ;;
        esac
        n=$((n + 1))
    done
done
test "$n" -ge 20 || fail "only $n links found on the site"
ok "all $n links lead to a page and an anchor of the site, or to https"

# Nothing fetched from elsewhere: the stylesheet is the site's own, and
# there is no script or image at all.
if grep -Eq '<(script|img|iframe)' "$site"/*.html; then
    fail "the site loads a script, an image or a frame: $(grep -El '<(script|img|iframe)' "$site"/*.html)"
fi
if grep -o '<link [^>]*>' "$site"/*.html | grep -v 'href="mandoc.css"' | grep -q .; then
    fail "the site loads something other than its own stylesheet"
fi
ok "the site loads nothing but its own stylesheet"

# The pages link to each other, and the index to both.
grep -q '<a href="bmb_report.html"><b>bmb_report</b>(1)</a>' "$site/blas-microbenchmark.html" \
    || fail "blas-microbenchmark.html does not link bmb_report(1) to bmb_report.html"
grep -q '<a href="blas-microbenchmark.html"><b>blas-microbenchmark</b>(1)</a>' "$site/bmb_report.html" \
    || fail "bmb_report.html does not link blas-microbenchmark(1) to blas-microbenchmark.html"
grep -q 'href="https://man7.org/linux/man-pages/man8/numactl.8.html"' "$site/blas-microbenchmark.html" \
    || fail "numactl(8) is not linked to man7.org"
for p in blas-microbenchmark bmb_report; do
    grep -q "<a href=\"$p.html\">" "$site/index.html" || fail "the index does not link to $p.html"
    grep -q '<a href="https://github.com/MathieuCAISSA/blas-microbenchmark">' "$site/$p.html" \
        || fail "the project's URL in $p.html is not a link"
done
ok "the pages link to each other, to man7.org for the rest, to the project; the index to both"

# What the man page says, the page on the site says: every option.
for opt in $("$BENCH" --help | grep -oE '^  -[a-zA-Z], --[a-z0-9-]+' | sed 's/.*, //'); do
    grep -q -- "$opt" "$site/blas-microbenchmark.html" || fail "blas-microbenchmark.html does not have $opt"
done
for opt in $("$REPORT" --help | grep -oE '^  -[a-zA-Z], --[a-z0-9-]+' | sed 's/.*, //'); do
    grep -q -- "$opt" "$site/bmb_report.html" || fail "bmb_report.html does not have $opt"
done
ok "each page on the site has every option of its program's --help"

rm -rf "$T"
echo "all checks passed"
