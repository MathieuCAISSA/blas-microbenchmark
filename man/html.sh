#!/bin/sh
# Builds the documentation site from the man pages:
#
#     html.sh OUTDIR VERSION PAGE.1...
#
# writes OUTDIR/<page>.html for each page, an index.html, and site.css.
# `make html` runs it; the Pages workflow publishes the result.
#
# The pages are the man pages and nothing else -- the same source, checked
# by the same tests -- so the site cannot say anything the man pages do
# not. mandoc converts each one; this wraps it in the site's layout (a bar
# to move between pages, a table of contents built from its sections) and
# does what mandoc leaves undone for man(7) pages: a reference such as
# bmb_report(1) becomes a link, to the page on this site when there is one
# and to man7.org otherwise, and so does a URL in the text. (The pages
# could mark URLs with .UR, but groff then shows only the link text in a
# terminal, and the URL is lost.)
#
# It also writes outputs.html, which shows what the tools produce: a
# benchmark's table, CSV and JSON, run by this script as the page is built
# -- so they are always this version's, on the machine that built it --
# and screenshots of a report. BENCH_DIR is the directory holding the
# built benchmarks (level1, level2, level3).
#
# And development.html: every test and every CI job, described by their
# own sources (devpage.sh, sourced from here); TOP is the top of the
# source tree, by default srcdir's parent.
#
# Needs mandoc (MANDOC overrides the command). srcdir locates site.css. If
# IMAGES names a directory holding the screenshots -- doc/images in a git
# checkout -- the index and outputs.html show them; a release tarball has
# no such directory, and the pages go without.
set -e

if [ $# -lt 3 ]; then
    echo "usage: $0 OUTDIR VERSION PAGE.1..." >&2
    exit 2
fi
out=$1
version=$2
shift 2
MANDOC=${MANDOC:-mandoc}
if ! command -v "$MANDOC" >/dev/null 2>&1; then
    echo "$0: $MANDOC not found; the site is built with mandoc" >&2
    exit 1
fi
REPO=https://github.com/MathieuCAISSA/blas-microbenchmark
TOP=${TOP:-${srcdir:-.}/..}
. "${srcdir:-.}/devpage.sh"
BENCH_DIR=${BENCH_DIR:-../src/c}
for l in 1 2 3; do
    if [ ! -d "$BENCH_DIR/level$l" ]; then
        echo "$0: no benchmarks in $BENCH_DIR/level$l; build them first (make)" >&2
        exit 1
    fi
done
BENCH_PATH=$(cd "$BENCH_DIR" && pwd)
BENCH_PATH=$BENCH_PATH/level1:$BENCH_PATH/level2:$BENCH_PATH/level3

# The screenshots each page can show, light and dark; none without IMAGES.
have_image() {
    [ -n "$IMAGES" ] && [ -f "$IMAGES/$1-light.png" ] && [ -f "$IMAGES/$1-dark.png" ]
}

rm -rf "$out"
mkdir -p "$out"
cp "${srcdir:-.}/site.css" "$out/site.css"

# The names of this site's pages, to link to them rather than to man7.org.
own=
for page; do
    own="$own $(basename "$page" .1)"
done

# A URL in the text becomes a link (one already in an attribute, after a
# quote, is left alone). <b>name</b>(N), which is how mandoc renders
# ".BR name (N)", becomes a link to man7.org, then is pointed back here
# for this site's own pages.
link_references() {
    script='s#\([^"]\)\(https://[A-Za-z0-9./_-]*[A-Za-z0-9/_-]\)#\1<a href="\2">\2</a>#g
s#<b>\([A-Za-z0-9_.-]*\)</b>(\([1-9]\))#<a href="https://man7.org/linux/man-pages/man\2/\1.\2.html"><b>\1</b>(\2)</a>#g'
    for n in $own; do
        script="$script
s#https://man7.org/linux/man-pages/man1/$n.1.html#$n.html#g"
    done
    sed "$script"
}

# The text after "\-" in a page's NAME section: its one-line description.
description() {
    sed -n '/^\.SH NAME/,/^\.SH /p' "$1" | sed '1d;$d' | tr '\n' ' ' \
        | sed 's/.*\\- *//; s/\\-/-/g; s/ *$//'
}

# mandoc's fragment without what the site's own layout already says: the
# header and footer tables (the bar and the footer), and the NAME section
# (the page's title and description, right above it).
strip_repeats() {
    awk '/^<table class="(head|foot)">/ { skip = "table" }
         /^<section class="Sh">$/ { held = $0; next }
         held != "" { if ($0 ~ /id="NAME"/) { skip = "section" } else if (skip == "") { print held }; held = "" }
         skip == "" { print }
         skip == "table" && /^<\/table>/ { skip = "" }
         skip == "section" && /^<\/section>/ { skip = "" }'
}

# The table of contents: an entry per section (h1.Sh), with its subsections
# (h2.Ss) nested in it, each linking to the id mandoc gave the heading.
# A heading can span lines, so the page is read whole.
toc() {
    awk '{ text = text $0 "\n" }
    END {
        print "<ul>"
        n = 0
        while (match(text, /<h[12] class="S[hs][^"]*" id="[^"]*"><a class="permalink" href="#[^"]*">[^<]*<\/a>/)) {
            h = substr(text, RSTART, RLENGTH)
            text = substr(text, RSTART + RLENGTH)
            id = h; sub(/^.* id="/, "", id); sub(/".*/, "", id)
            name = h; sub(/.*">/, "", name); sub(/<\/a>$/, "", name); gsub(/[ \n]+/, " ", name)
            keep = (h ~ /keep-case/) ? " keep-case" : ""
            if (substr(h, 3, 1) == "1") {
                if (sub_open) { print "    </ul>"; sub_open = 0 }
                if (n++) { print "  </li>" }
                printf "  <li class=\"toc-sh%s\"><a href=\"#%s\">%s</a>\n", keep, id, name
            } else {
                if (!sub_open) { print "    <ul>"; sub_open = 1 }
                printf "      <li class=\"toc-ss\"><a href=\"#%s\">%s</a></li>\n", id, name
            }
        }
        if (sub_open) { print "    </ul>" }
        if (n) { print "  </li>" }
        print "</ul>"
    }'
}

# topbar CURRENT: the bar at the top of every page; CURRENT, the page
# shown, is marked for the reader.
topbar() {
    echo '<header class="topbar"><div class="topbar-inner">'
    echo "  <a class=\"brand\" href=\"index.html\">blas-microbenchmark<span class=\"version\">$version</span></a>"
    echo '  <nav class="topnav" aria-label="Pages">'
    for n in $own; do
        current=
        [ "$n" = "$1" ] && current=' aria-current="page"'
        echo "    <a href=\"$n.html\"$current>$n(1)</a>"
    done
    for p in outputs:Outputs development:Development; do
        current=
        [ "$1" = "${p%%:*}" ] && current=' aria-current="page"'
        echo "    <a href=\"${p%%:*}.html\"$current>${p#*:}</a>"
    done
    echo "    <a href=\"$REPO\">GitHub</a>"
    echo '  </nav>'
    echo '</div></header>'
}

# picture NAME ALT CAPTION: a screenshot, light or dark with the reader's
# theme, copied into the site.
picture() {
    cp "$IMAGES/$1-light.png" "$IMAGES/$1-dark.png" "$out/"
    cat <<EOF
<figure class="shot">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="$1-dark.png"/>
    <img src="$1-light.png" alt="$2" loading="lazy"/>
  </picture>
  <figcaption>$3</figcaption>
</figure>
EOF
}

escape_html() {
    sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g'
}

# run COMMAND [FILE]: runs COMMAND, a benchmark command line, in a scratch
# directory with the benchmarks on the PATH, and shows it as typed with
# what it printed -- or, given FILE, the file it wrote. The command shown
# is the command run.
run() {
    mkdir -p "$out/.run"
    (cd "$out/.run" && PATH=$BENCH_PATH:$PATH && eval "$1") >"$out/.run/stdout" 2>"$out/.run/stderr" \
        || { echo "$0: \"$1\" failed: $(cat "$out/.run/stderr")" >&2; exit 1; }
    echo '<div class="Bd-indent"><pre>'
    printf '$ %s\n' "$1" | escape_html
    if [ -n "$2" ]; then
        printf '$ cat %s\n' "$2" | escape_html
        escape_html <"$out/.run/$2"
    else
        escape_html <"$out/.run/stdout"
    fi
    echo '</pre></div>'
}

# heading LEVEL ID TEXT: a section heading, as mandoc writes them, so the
# table of contents finds it; keep-case leaves its capitals alone.
heading() {
    if [ "$1" = 1 ]; then
        echo "<h1 class=\"Sh keep-case\" id=\"$2\"><a class=\"permalink\" href=\"#$2\">$3</a></h1>"
    else
        echo "<h2 class=\"Ss keep-case\" id=\"$2\"><a class=\"permalink\" href=\"#$2\">$3</a></h2>"
    fi
}

# head_html TITLE DESCRIPTION
head_html() {
    cat <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <meta name="description" content="$2"/>
  <link rel="stylesheet" href="site.css"/>
  <title>$1</title>
</head>
<body>
EOF
}

# footer_html EXTRA: the footer of every page, EXTRA after the version.
footer_html() {
    cat <<EOF
<footer class="site-footer"><div class="site-footer-inner">
  blas-microbenchmark $version$1 &middot; built from the <code>main</code> branch &middot;
  <a href="$REPO">source on GitHub</a>
</div></footer>
</body>
</html>
EOF
}

for page; do
    name=$(basename "$page" .1)
    desc=$(description "$page")
    # Through a file, not a pipe: in a pipe, a mandoc failure would be
    # hidden behind the next command's success, and leave an empty page.
    "$MANDOC" -Thtml -Ofragment "$page" >"$out/$name.tmp"
    test -s "$out/$name.tmp" || { echo "$0: mandoc produced nothing for $page" >&2; exit 1; }
    date=$(sed -n 's/.*<td class="foot-date">\([^<]*\)<\/td>.*/\1/p' "$out/$name.tmp")
    strip_repeats <"$out/$name.tmp" | link_references >"$out/$name.body"
    {
        head_html "$name(1) &mdash; blas-microbenchmark" "$name: $desc"
        topbar "$name"
        echo '<div class="layout">'
        echo '<nav class="toc" aria-label="On this page">'
        echo '<p class="toc-title">On this page</p>'
        toc <"$out/$name.body"
        echo '</nav>'
        echo '<main class="page">'
        echo "<h1 class=\"page-title\">$name<span class=\"mansect\">(1)</span></h1>"
        echo "<p class=\"page-desc\">$desc</p>"
        cat "$out/$name.body"
        echo '</main>'
        echo '</div>'
        footer_html "${date:+ &middot; page dated $date}"
    } >"$out/$name.html"
    rm -f "$out/$name.tmp" "$out/$name.body"
done

# What the tools produce.
{
    head_html "Outputs &mdash; blas-microbenchmark" "What blas-microbenchmark and bmb_report produce"
    topbar outputs
    echo '<div class="layout">'
    echo '<nav class="toc" aria-label="On this page">'
    echo '<p class="toc-title">On this page</p>'
    {
        echo '<section class="Sh">'
        heading 1 terminal "In the terminal"
        echo '<p class="Pp">A benchmark prints where the result came from, then one row per point as it is measured: here dgemm over three sizes, at one and two threads. These runs were made when this page was built, by the machine that built it.</p>'
        run "bmb_dgemm -m 256:1024 -t 1,2"
        echo '</section>'
        echo '<section class="Sh">'
        heading 1 csv "CSV"
        echo '<p class="Pp">With <b>-o</b>, the same results go to a file as well; <b>-s</b> adds the mean, the standard deviation, the slowest sample and the batch size. The comment lines say where the result came from; skip them when reading the file.</p>'
        run "bmb_ddot -v 1024:8192 -s -o ddot.csv" ddot.csv
        echo '</section>'
        echo '<section class="Sh">'
        heading 1 json "JSON"
        echo '<p class="Pp">The JSON holds the same as fields, and is what <a href="bmb_report.html"><b>bmb_report</b>(1)</a> reads. Here with a label, to tell two runs on the same machine apart.</p>'
        run "bmb_dgemv -m 512 -M 256:512 -l 'turbo off' -o dgemv.json" dgemv.json
        echo '</section>'
        if have_image summary && have_image rawdata && have_image ddot-size && have_image dgemm-ratio \
            && have_image dgemm-threads && have_image dgemv-shapes; then
            echo '<section class="Sh">'
            heading 1 report "The report"
            echo '<p class="Pp"><code>bmb_report results/*.json &gt; report.html</code> makes one page of all of them. These are from results measured on a laptop (Intel Core Ultra 7 155U, WSL2), with OpenBLAS 0.3.26 and BLIS 0.9.0: they show what the page draws, not which library is faster.</p>'
            heading 2 summary "Summary"
            echo '<p class="Pp">The best figure for each routine, and where every series came from.</p>'
            picture summary "The report's summary: the best GFLOP/s for ddot, dgemv and dgemm, and a table of the library, CPU, caches, OS, date and files of each series" "The summary and the provenance of each series."
            heading 2 charts "Charts"
            echo '<p class="Pp">Hovering a chart gives the exact values.</p>'
            echo '<div class="shots-grid">'
            picture ddot-size "ddot GFLOP/s against vector size, OpenBLAS and BLIS, with the L1d, L2 and L3 sizes marked" "Performance against size, the caches marked."
            picture dgemm-ratio "dgemm, BLIS relative to OpenBLAS at 8 threads, on a log scale around x1" "Each library against the reference."
            picture dgemm-threads "dgemm GFLOP/s at 1, 2, 4 and 8 threads, with ideal scaling dashed" "Thread scaling, against ideal."
            picture dgemv-shapes "dgemv heatmaps of GFLOP/s over M and N, OpenBLAS and BLIS" "Every shape of a two-dimension sweep."
            echo '</div>'
            heading 2 raw "Raw data"
            echo '<p class="Pp">Every row, sortable by any column.</p>'
            picture rawdata "The report's raw data table: routine, series, threads, sizes, time, GFLOP/s, GB/s and file of each point" "The raw data table, opened."
            echo '</section>'
        fi
    } >"$out/outputs.body"
    toc <"$out/outputs.body"
    echo '</nav>'
    echo '<main class="page wide">'
    echo '<h1 class="page-title">Outputs</h1>'
    echo '<p class="page-desc">What the benchmarks print and save, and the page bmb_report makes of it.</p>'
    echo '<div class="manual-text">'
    cat "$out/outputs.body"
    echo '</div>'
    echo '</main>'
    echo '</div>'
    footer_html ""
} >"$out/outputs.html"
rm -rf "$out/outputs.body" "$out/.run"

# How it is tested.
{
    head_html "Development &mdash; blas-microbenchmark" "How blas-microbenchmark is tested, and what its CI runs"
    topbar development
    development_body "$TOP" >"$out/development.body"
    echo '<div class="layout">'
    echo '<nav class="toc" aria-label="On this page">'
    echo '<p class="toc-title">On this page</p>'
    toc <"$out/development.body"
    echo '</nav>'
    echo '<main class="page wide">'
    echo '<h1 class="page-title">Development</h1>'
    echo "<p class=\"page-desc\">How it is tested, and what CI runs. To work on it, start with <a href=\"$REPO/blob/main/AGENTS.md\">AGENTS.md</a>.</p>"
    echo '<div class="manual-text">'
    cat "$out/development.body"
    echo '</div>'
    echo '</main>'
    echo '</div>'
    footer_html ""
} >"$out/development.html"
rm -f "$out/development.body"

# The index: what the project is, a card per page, and the chart.
first=$(basename "$1" .1)
{
    head_html "blas-microbenchmark $version" "$(description "$1")"
    topbar ""
    echo '<main class="landing">'
    echo '<div class="hero">'
    echo '  <h1>blas-microbenchmark</h1>'
    echo "  <p class=\"tagline\">$(description "$1"): one small program per routine, the same options for all of them, and a page of charts to compare libraries.</p>"
    echo '  <div class="actions">'
    echo "    <a class=\"button primary\" href=\"$first.html\">Read the manual</a>"
    echo '    <a class="button" href="outputs.html">See the outputs</a>'
    echo "    <a class=\"button\" href=\"$REPO#install\">Install</a>"
    echo "    <a class=\"button\" href=\"$REPO\">GitHub</a>"
    echo '  </div>'
    echo '</div>'
    echo '<div class="cards">'
    for page; do
        n=$(basename "$page" .1)
        echo "  <a class=\"card\" href=\"$n.html\">"
        echo "    <div class=\"card-title\">$n(1)</div>"
        echo "    <p class=\"card-desc\">$(description "$page")</p>"
        echo "    <p class=\"card-cmd\">man $n</p>"
        echo '  </a>'
    done
    echo '  <a class="card" href="outputs.html">'
    echo '    <div class="card-title">Outputs</div>'
    echo '    <p class="card-desc">What the benchmarks print and save, and the report they make</p>'
    echo '    <p class="card-cmd">table, CSV, JSON, charts</p>'
    echo '  </a>'
    echo '  <a class="card" href="development.html">'
    echo '    <div class="card-title">Development</div>'
    echo '    <p class="card-desc">How it is tested, and what CI runs</p>'
    echo '    <p class="card-cmd">make check</p>'
    echo '  </a>'
    echo '</div>'
    if have_image ddot-size; then
        picture ddot-size "A chart from bmb_report: ddot GFLOP/s against vector size for OpenBLAS and BLIS, with the L1, L2 and L3 sizes marked" \
            'A chart from <a href="bmb_report.html">bmb_report</a>: ddot, OpenBLAS against BLIS, with the caches marked. <a href="outputs.html">More of what it shows</a>.'
    fi
    echo "<p class=\"note\">These are the manual pages of blas-microbenchmark $version, also installed with it, built from its <code>main</code> branch.</p>"
    echo '</main>'
    footer_html ""
} >"$out/index.html"
