#!/bin/sh
# Checks the documentation site html.sh builds from the man pages: every
# page and the index are there, every link on the site leads somewhere --
# to a file and an anchor that exist, or to https -- nothing is loaded
# from elsewhere, the pages link to each other, and each page holds what
# its man page does (every option of --help, the version).
set -e
. "${srcdir:-.}/man_helper.sh"
need mandoc "build the site"

# With the chart when the checkout has it (a release tarball does not):
# the checks below hold either way.
site=$T/site
IMAGES=${srcdir:-.}/../doc/images srcdir=${srcdir:-.} \
    "${SHELL:-/bin/sh}" "${srcdir:-.}/html.sh" "$site" "$PACKAGE_VERSION" $PAGES \
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

if BENCH_DIR="$T/nowhere" srcdir=${srcdir:-.} "${SHELL:-/bin/sh}" "${srcdir:-.}/html.sh" "$T/bad" "$PACKAGE_VERSION" $PAGES \
    >/dev/null 2>&1; then
    fail "html.sh succeeded without the benchmarks it runs for outputs.html"
fi
ok "html.sh fails when the benchmarks are not built"

for f in index.html site.css blas-microbenchmark.html bmb_report.html outputs.html development.html; do
    test -s "$site/$f" || fail "the site has no $f"
done
ok "the site has the index, the stylesheet, both pages, the outputs and the development page"

# development.html: every test the Makefiles run is on it -- the
# per-routine smoke tests as one entry, counted -- and, in a git checkout,
# every job of the CI workflow.
top=${srcdir:-.}/..
d=$site/development.html
n=0
for t in $(grep -ho 'test_[a-z0-9_]*\(\.sh\|\.js\)\{0,1\}' "$top"/src/c/Makefile.am "$top"/src/c/*/Makefile.am \
           "$top"/src/report/Makefile.am "$top"/Makefile.am \
           | sed 's/_$//' | grep -v '^test_helper' | sort -u); do
    case $t in
        test_bmb_*.sh) r=${t#test_bmb_}; r=${r%.sh}
                       case " $(echo $ROUTINES) " in *" $r "*) continue ;; esac ;;
    esac
    case $t in *.sh | *.js) ;; *) t=$t.c ;; esac
    grep -q "<dt><code>$t</code>" "$d" || fail "development.html does not describe $t"
    n=$((n + 1))
done
for t in "$top"/man/test_man_*.sh; do
    grep -q "<dt><code>$(basename "$t")</code>" "$d" || fail "development.html does not describe $(basename "$t")"
    n=$((n + 1))
done
grep -q "$(echo $ROUTINES | wc -w) of them" "$d" || fail "development.html does not count the $(echo $ROUTINES | wc -w) smoke tests"
if [ -f "$top/.github/workflows/ci.yml" ]; then
    for job in $(sed -n '/^jobs:/,$ s/^  \([A-Za-z0-9_-]*\):$/\1/p' "$top/.github/workflows/ci.yml"); do
        grep -q "<dt><code>$job</code>" "$d" || fail "development.html does not describe the CI job $job"
        n=$((n + 1))
    done
fi
ok "development.html describes all $n tests and CI jobs, and the smoke tests"

# The outputs are runs of this version, made as the page was built: the
# table, the CSV and the JSON each carry its version, and the commands
# shown are benchmarks.
o=$site/outputs.html
for id in terminal csv json; do
    grep -q "id=\"$id\"" "$o" || fail "outputs.html has no $id section"
done
test "$(grep -c "^# blas-microbenchmark $PACKAGE_VERSION\$" "$o")" -ge 2 \
    || fail "outputs.html does not show this version's table and CSV"
grep -q "^  \"version\": \"$PACKAGE_VERSION\",\$" "$o" || fail "outputs.html does not show this version's JSON"
test "$(grep -c '^\$ bmb_d[a-z0-9]* ' "$o")" -eq 3 || fail "outputs.html does not show the three benchmark commands"
grep -q '^Thread count' "$o" || fail "outputs.html shows no table header"
grep -q '^thread_count,' "$o" || fail "outputs.html shows no CSV header"
# The report's screenshots come with a git checkout (doc/images), not with
# a release tarball.
if [ -d "${srcdir:-.}/../doc/images" ]; then
    for id in report summary charts noise raw; do
        grep -q "id=\"$id\"" "$o" || fail "outputs.html has no $id section, although doc/images is there"
    done
    test "$(grep -c '<img ' "$o")" -eq 8 || fail "outputs.html does not show the eight screenshots of the report"
fi
ok "outputs.html shows this version's table, CSV and JSON, and the report when doc/images is there"

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

# Nothing fetched from elsewhere: no script or frame at all, the site's own
# stylesheet, and images (src, srcset) that are files of the site.
if grep -Eq '<(script|iframe)' "$site"/*.html; then
    fail "the site has a script or a frame: $(grep -El '<(script|iframe)' "$site"/*.html)"
fi
if grep -o '<link [^>]*>' "$site"/*.html | grep -v 'href="site.css"' | grep -q .; then
    fail "the site loads something other than its own stylesheet"
fi
images=0
for f in "$site"/*.html; do
    for src in $(grep -oE '(src|srcset)="[^"]*"' "$f" | sed 's/^[a-z]*="//; s/"$//'); do
        case $src in
            */* | *:*) fail "$(basename "$f") loads $src, which is not a file of the site" ;;
        esac
        test -f "$site/$src" || fail "$(basename "$f") loads $src, and the site has no such file"
        images=$((images + 1))
    done
done
ok "the site loads nothing but its own stylesheet and $images images of its own"

# Every section of a page is in its table of contents.
for p in blas-microbenchmark bmb_report; do
    for id in $(grep -o '<h[12] class="S[hs]" id="[^"]*"' "$site/$p.html" | sed 's/.* id="//; s/"$//'); do
        sed -n '/<nav class="toc"/,/<\/nav>/p' "$site/$p.html" | grep -q "href=\"#$id\"" \
            || fail "$p.html: section $id is not in the table of contents"
    done
done
ok "every section and subsection is in its page's table of contents"

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
