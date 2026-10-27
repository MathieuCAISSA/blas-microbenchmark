#!/bin/sh
# Builds the documentation site:
#
#     html.sh OUTDIR VERSION PAGE.1...
#
# writes OUTDIR/<page>.html for each page of the site, and site.css. `make
# html` runs it; the Pages workflow publishes the result.
#
# The site is a menu (MENU below) of four kinds of page:
#
# - The guides, site/<name>.html beside this script: written for the site,
#   each a fragment of HTML whose first lines give its title and
#   description (<!-- title: ... -->, <!-- description: ... -->). Their
#   headings are <h2 id=...> and <h3 id=...>; their commands are checked
#   against the programs by test_man_html.sh, as the man pages' examples
#   are by test_man_content.sh.
# - The man pages, converted by mandoc. blas-microbenchmark(1) is long, so
#   each of its sections goes to a page of its own (manual_page_of), and a
#   link into it (blas-microbenchmark.html#ID) is pointed at the page that
#   holds ID. The pages are the man pages and nothing else -- the same
#   source, checked by the same tests -- so the manual on the site cannot
#   say anything the installed one does not. This adds what mandoc leaves
#   undone for man(7) pages: a reference such as bmb_report(1) becomes a
#   link, to the page on this site when there is one and to man7.org
#   otherwise, and so does a URL in the text. (The pages could mark URLs
#   with .UR, but groff then shows only the link text in a terminal, and
#   the URL is lost.)
# - outputs.html, which shows what the tools produce: a benchmark's table,
#   CSV and JSON, run by this script as the page is built -- so they are
#   always this version's, on the machine that built it -- and screenshots
#   of a report. BENCH_DIR is the directory holding the built benchmarks
#   (level1, level2, level3, each with a directory per precision in a
#   build, or the benchmarks themselves where they are installed).
# - development.html: every test and every CI job, described by their
#   own sources (devpage.sh, sourced from here); TOP is the top of the
#   source tree, by default srcdir's parent.
#
# Every page has the menu, its own table of contents, and links to the
# pages before and after it. Needs mandoc (MANDOC overrides the command).
# srcdir locates site.css and site/. If IMAGES names a directory holding
# the screenshots -- doc/images in a git checkout -- the pages show them; a
# release tarball has no such directory, and the pages go without.
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
# Each level's benchmarks: in a build, in a directory per precision
# (level3/double/bmb_dgemm); installed, in the level's own (level3/bmb_dgemm).
BENCH_PATH=
for d in "$(cd "$BENCH_DIR" && pwd)"/level[123] "$(cd "$BENCH_DIR" && pwd)"/level[123]/*/; do
    d=${d%/}
    for b in "$d"/bmb_*; do
        if [ -f "$b" ] && [ -x "$b" ]; then
            BENCH_PATH=${BENCH_PATH:+$BENCH_PATH:}$d
            break
        fi
    done
done

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

# The menu, in order: "group|page|title" for each page of the site. The
# guides are site/<page>.html; the manual's pages are cut from
# blas-microbenchmark(1) by manual_page_of; the rest are built below.
MENU='Getting started|index|Home
Getting started|install|Install
Getting started|first-run|Your first benchmarks
Guides|compare|Compare libraries
Guides|backends|The BLAS libraries
Guides|hpc|On an HPC cluster
Guides|reframe|ReFrame tests
The benchmarks|blas-microbenchmark|Overview
The benchmarks|options|Options
The benchmarks|measuring|How a number is measured
The benchmarks|checking|Checking results
The benchmarks|threads|Threads
The benchmarks|counters|Hardware counters
The benchmarks|output|Output
The benchmarks|environment|Exit status and environment
The benchmarks|examples|Examples
Run and report|bmb_suite|bmb_suite
Run and report|bmb_report|bmb_report
Run and report|outputs|What they produce
Project|development|Development'

# Which page each section of blas-microbenchmark(1) goes to; a section
# with no page here stops the build, rather than vanish from the site.
manual_page_of() {
    case $1 in
        SYNOPSIS | DESCRIPTION | SEE_ALSO) echo blas-microbenchmark ;;
        OPTIONS) echo options ;;
        HOW_A_NUMBER_IS_MEASURED) echo measuring ;;
        CHECKING_RESULTS) echo checking ;;
        THREADS) echo threads ;;
        HARDWARE_COUNTERS) echo counters ;;
        OUTPUT) echo output ;;
        EXIT_STATUS | ENVIRONMENT | FILES) echo environment ;;
        EXAMPLES) echo examples ;;
        *) return 1 ;;
    esac
}

# title_of PAGE: its title in the menu.
title_of() {
    printf '%s\n' "$MENU" | awk -F'|' -v p="$1" '$2 == p { print $3; exit }'
}

# menu CURRENT: the site's menu, by group, CURRENT marked for the reader.
menu() {
    printf '%s\n' "$MENU" | awk -F'|' -v cur="$1" -v repo="$REPO" '
        $1 != group {
            if (group != "") print "  </ul>"
            group = $1
            printf "  <p class=\"menu-group\">%s</p>\n  <ul>\n", group
        }
        { printf "    <li><a href=\"%s.html\"%s>%s</a></li>\n", $2, ($2 == cur ? " aria-current=\"page\"" : ""), $3 }
        END {
            printf "    <li><a href=\"%s/blob/main/CHANGELOG.md\">Changelog</a></li>\n", repo
            printf "    <li><a href=\"%s\">GitHub</a></li>\n", repo
            print "  </ul>"
        }'
}

# topbar CURRENT: the bar at the top of every page, and the menu: beside
# the page on a wide screen, folded under "Menu" in the bar on a narrow one.
topbar() {
    echo '<header class="topbar"><div class="topbar-inner">'
    echo "  <a class=\"brand\" href=\"index.html\">blas-microbenchmark<span class=\"version\">$version</span></a>"
    echo '  <nav class="topnav" aria-label="Main">'
    for p in install:Install first-run:'Get started' blas-microbenchmark:Manual bmb_report:Report; do
        current=
        [ "$1" = "${p%%:*}" ] && current=' aria-current="page"'
        echo "    <a href=\"${p%%:*}.html\"$current>${p#*:}</a>"
    done
    echo "    <a href=\"$REPO\">GitHub</a>"
    echo '  </nav>'
    echo '  <details class="menu-mobile"><summary>Menu</summary>'
    echo '  <nav class="menu" aria-label="Site">'
    menu "$1"
    echo '  </nav>'
    echo '  </details>'
    echo '</div></header>'
}

# neighbours PAGE: links to the pages before and after PAGE in the menu.
neighbours() {
    printf '%s\n' "$MENU" | awk -F'|' -v cur="$1" '
        { page[NR] = $2; title[NR] = $3; if ($2 == cur) at = NR }
        END {
            print "<nav class=\"neighbours\" aria-label=\"Previous and next\">"
            if (at > 1) printf "  <a class=\"prev\" href=\"%s.html\"><span>Previous</span>%s</a>\n", page[at - 1], title[at - 1]
            if (at < NR) printf "  <a class=\"next\" href=\"%s.html\"><span>Next</span>%s</a>\n", page[at + 1], title[at + 1]
            print "</nav>"
        }'
}

# write_page PAGE TITLE DESC BODY [FOOTER]: OUTDIR/PAGE.html, with the
# menu on the left, BODY's table of contents on the right, the title and
# DESC above BODY, the neighbours below it.
write_page() {
    {
        head_html "$2 &mdash; blas-microbenchmark" "$3"
        topbar "$1"
        echo '<div class="layout">'
        echo '<nav class="menu sidebar" aria-label="Site">'
        menu "$1"
        echo '</nav>'
        echo '<main class="page">'
        echo "<h1 class=\"page-title\">$2</h1>"
        echo "<p class=\"page-desc\">$3</p>"
        echo '<div class="manual-text">'
        cat "$4"
        echo '</div>'
        neighbours "$1"
        echo '</main>'
        echo '<nav class="toc" aria-label="On this page">'
        echo '<p class="toc-title">On this page</p>'
        toc <"$4"
        echo '</nav>'
        echo '</div>'
        footer_html "$5"
    } >"$out/$1.html"
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

# The man pages: bmb_report(1) and bmb_suite(1) a page each, blas-microbenchmark(1) cut
# into a page per section, or per few small ones.
for page; do
    name=$(basename "$page" .1)
    desc=$(description "$page")
    # Through a file, not a pipe: in a pipe, a mandoc failure would be
    # hidden behind the next command's success, and leave an empty page.
    "$MANDOC" -Thtml -Ofragment "$page" >"$out/$name.tmp"
    test -s "$out/$name.tmp" || { echo "$0: mandoc produced nothing for $page" >&2; exit 1; }
    date=$(sed -n 's/.*<td class="foot-date">\([^<]*\)<\/td>.*/\1/p' "$out/$name.tmp")
    strip_repeats <"$out/$name.tmp" | link_references >"$out/$name.body"
    rm -f "$out/$name.tmp"
    if [ "$name" != blas-microbenchmark ]; then
        write_page "$name" "$name" "$desc" "$out/$name.body" "${date:+ &middot; page dated $date}"
        rm -f "$out/$name.body"
        continue
    fi
    # Each top-level section to the body of its page, in the page's order.
    mkdir -p "$out/.manual"
    awk -v dir="$out/.manual" '
        /^<section class="Sh">$/ && depth == 0 { held = $0; depth = 1; next }
        held != "" {
            id = $0; sub(/.* id="/, "", id); sub(/".*/, "", id)
            print id > (dir "/order")
            print held > (dir "/" id ".section")
            held = ""; cur = dir "/" id ".section"
        }
        /^<section/ && cur != "" && !/^<section class="Sh">$/ { depth++ }
        cur != "" { print > cur }
        /^<\/section>$/ && cur != "" { if (--depth == 0) { close(cur); cur = "" } }
    ' "$out/$name.body"
    sections=$(cat "$out/.manual/order")
    for id in $sections; do
        p=$(manual_page_of "$id") || { echo "$0: no page of the site for the section $id of $page; add it to manual_page_of" >&2; exit 1; }
        cat "$out/.manual/$id.section" >>"$out/.manual/$p.body"
    done
    for p in $(printf '%s\n' "$MENU" | awk -F'|' '$1 == "The benchmarks" { print $2 }'); do
        test -s "$out/.manual/$p.body" || { echo "$0: no section of $page went to $p.html" >&2; exit 1; }
        if [ "$p" = blas-microbenchmark ]; then
            pdesc=$desc
        else
            pdesc="From the manual of the benchmarks, <code>man blas-microbenchmark</code>"
        fi
        # A page of one section has its title already: the section's own
        # heading goes, its id stays for the links to it.
        if [ "$(grep -c '^<h1 class="Sh"' "$out/.manual/$p.body")" -eq 1 ]; then
            sed -i 's#^<h1 class="Sh" id="\([^"]*\)"><a class="permalink" href="\#[^"]*">[^<]*</a></h1>$#<span id="\1"></span>#' \
                "$out/.manual/$p.body"
        fi
        write_page "$p" "$(title_of "$p")" "$pdesc" "$out/.manual/$p.body" "${date:+ &middot; page dated $date}"
    done
    rm -rf "$out/.manual" "$out/$name.body"
done

# The guides: their headings as the manual's, so that the table of
# contents and the stylesheet treat them alike.
for g in $(printf '%s\n' "$MENU" | awk -F'|' '$1 == "Getting started" || $1 == "Guides" { print $2 }'); do
    [ "$g" = index ] && continue
    src=${srcdir:-.}/site/$g.html
    test -s "$src" || { echo "$0: no guide $src" >&2; exit 1; }
    gdesc=$(sed -n 's/^<!-- description: \(.*\) -->$/\1/p' "$src")
    test -n "$gdesc" || { echo "$0: $src has no <!-- description: ... --> line" >&2; exit 1; }
    sed -e '/^<!-- [a-z]*: .* -->$/d' \
        -e 's#<h2 id="\([^"]*\)">\(.*\)</h2>#<h1 class="Sh keep-case" id="\1"><a class="permalink" href="\#\1">\2</a></h1>#' \
        -e 's#<h3 id="\([^"]*\)">\(.*\)</h3>#<h2 class="Ss keep-case" id="\1"><a class="permalink" href="\#\1">\2</a></h2>#' \
        "$src" >"$out/$g.body"
    # A screenshot, by a line <!-- picture NAME | ALT | CAPTION -->; gone
    # without IMAGES.
    grep '^<!-- picture ' "$out/$g.body" | while IFS='|' read -r shot alt caption; do
        shot=$(echo "${shot#<!-- picture }" | tr -d ' ')
        alt=$(echo "$alt" | sed 's/^ *//; s/ *$//')
        caption=$(echo "$caption" | sed 's/ *-->$//; s/^ *//')
        if have_image "$shot"; then
            picture "$shot" "$alt" "$caption" >"$out/$g.$shot.shot"
        else
            : >"$out/$g.$shot.shot"
        fi
    done
    for f in "$out/$g".*.shot; do
        [ -f "$f" ] || continue
        shot=${f#"$out/$g."}; shot=${shot%.shot}
        sed -i -e "/^<!-- picture $shot |/r $f" -e "/^<!-- picture $shot |/d" "$out/$g.body"
        rm -f "$f"
    done
    write_page "$g" "$(title_of "$g")" "$gdesc" "$out/$g.body"
    rm -f "$out/$g.body"
done
# What the tools produce.
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
        && have_image dgemm-threads && have_image dgemv-shapes && have_image noise-band \
        && have_image noise-ratio; then
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
        heading 2 noise "Repeated runs"
        echo '<p class="Pp">The same ddot command run four times with each library, each run into its own file: a band shows how far the runs spread, and the comparison tells a difference within that noise (a hollow point) from one beyond it (see <a href="bmb_report.html#Repeated_runs_and_noise">Repeated runs and noise</a>).</p>'
        echo '<div class="shots-grid">'
        picture noise-band "ddot GFLOP/s against vector size, OpenBLAS and BLIS, four runs each, with a band from the fastest run to the slowest" "The spread of four runs."
        picture noise-ratio "ddot, BLIS relative to OpenBLAS, four runs each: hollow points where the difference is within the noise, filled ones where it is beyond" "Hollow: within the noise."
        echo '</div>'
        heading 2 raw "Raw data"
        echo '<p class="Pp">Every row, sortable by any column.</p>'
        picture rawdata "The report's raw data table: routine, series, threads, sizes, time, GFLOP/s, GB/s and file of each point" "The raw data table, opened."
        echo '</section>'
    fi
} >"$out/outputs.body"
write_page outputs "$(title_of outputs)" "What the benchmarks print and save, and the page bmb_report makes of it." "$out/outputs.body"
rm -rf "$out/outputs.body" "$out/.run"

# How it is tested.
development_body "$TOP" >"$out/development.body"
write_page development "$(title_of development)" \
    "How it is tested, and what CI runs. To work on it, start with <a href=\"$REPO/blob/main/AGENTS.md\">AGENTS.md</a>." \
    "$out/development.body"
rm -f "$out/development.body"

# The home page: what it is for, the three steps, where to go next.
{
    head_html "blas-microbenchmark $version" "$(description "$1")"
    topbar index
    echo '<main class="landing">'
    echo '<div class="hero">'
    echo '  <h1>How fast is each BLAS routine on this machine, with this library?</h1>'
    echo '  <p class="tagline">blas-microbenchmark times BLAS routines one by one, checks every result before timing it, and puts the libraries side by side on one page of charts.</p>'
    echo '  <div class="actions">'
    echo '    <a class="button primary" href="install.html">Install</a>'
    echo '    <a class="button" href="first-run.html">Your first benchmarks</a>'
    echo '    <a class="button" href="compare.html">Compare libraries</a>'
    echo "    <a class=\"button\" href=\"$REPO\">GitHub</a>"
    echo '  </div>'
    echo '</div>'
    echo '<ol class="steps-row">'
    echo '  <li><span class="step-n">1</span><b>Install</b> against your BLAS: a release, Spack or a container.<pre>./configure &amp;&amp; make &amp;&amp; make install</pre></li>'
    echo '  <li><span class="step-n">2</span><b>Run</b> a routine over a sweep of sizes and threads.<pre>bmb_dgemm -m 512:4096 -t 1:8</pre></li>'
    echo '  <li><span class="step-n">3</span><b>Compare</b> libraries and runs on one page of charts.<pre>bmb_report *.json &gt; report.html</pre></li>'
    echo '</ol>'
    if have_image ddot-size; then
        picture ddot-size "A chart from bmb_report: ddot GFLOP/s against vector size for OpenBLAS and BLIS, with the L1, L2 and L3 sizes marked" \
            'ddot with OpenBLAS and BLIS, the caches marked: one of the charts <a href="compare.html">bmb_report</a> draws.'
    fi
    echo '<ul class="features">'
    echo '  <li><b>104 benchmarks</b>, one executable per BLAS routine, levels 1 to 3, in single, double and complex precision; the same options for all.</li>'
    echo '  <li><b>Numbers you can trust</b>: every point checked against a reference before it is timed; the CPU, its frequency, the library and its version recorded with it.</li>'
    echo '  <li><b>Eight BLAS libraries</b>: OpenBLAS, BLIS, AMD AOCL-BLAS, Intel MKL, NVPL, ArmPL, Netlib, or all those FlexiBLAS loads, from one build.</li>'
    echo '  <li><b>Made for HPC sites</b>: a Spack package, a Docker image that Apptainer runs, and ReFrame tests.</li>'
    echo '</ul>'
    echo '<h2 class="landing-h">Where to go next</h2>'
    echo '<div class="cards">'
    printf '%s\n' "$MENU" | awk -F'|' '$2 != "index" && ($1 == "Getting started" || $1 == "Guides" || $2 == "blas-microbenchmark" || $2 == "bmb_suite" || $2 == "bmb_report" || $2 == "outputs") { print $2 "|" $3 }' \
        | while IFS='|' read -r p t; do
            case $p in
                install) d="From a release, with Spack, in a container, from git; checking the download." ;;
                first-run) d="Run a routine, read what it prints, sweep sizes and threads, save the results." ;;
                compare) d="Measure several libraries, or runs, and see them side by side." ;;
                backends) d="The eight libraries it builds against, how each is found and threaded." ;;
                hpc) d="Modules, Spack, Slurm, pinning and NUMA, Apptainer." ;;
                reframe) d="Every benchmark as a ReFrame test, with your references." ;;
                blas-microbenchmark) d="The manual: every option, how a number is measured, threads, outputs." ;;
                bmb_suite) d="The whole suite, or a selection, at a profile's sizes, into one directory." ;;
                bmb_report) d="The manual of the report: series, charts, repeated runs and noise." ;;
                outputs) d="The table, CSV and JSON of a real run, and a report." ;;
                *) d= ;;
            esac
            echo "  <a class=\"card\" href=\"$p.html\"><div class=\"card-title\">$t</div><p class=\"card-desc\">$d</p></a>"
        done
    echo '</div>'
    echo "<p class=\"note\">The documentation of blas-microbenchmark $version, built from its <code>main</code> branch; its manual is also installed with it (<code>man blas-microbenchmark</code>).</p>"
    echo '</main>'
    footer_html ""
} >"$out/index.html"

# A link into blas-microbenchmark(1), from any page, to the page that now
# holds its target.
script=
for p in $(printf '%s\n' "$MENU" | awk -F'|' '$1 == "The benchmarks" { print $2 }'); do
    for id in $(grep -o ' id="[^"]*"' "$out/$p.html" | sed 's/^ id="//; s/"$//'); do
        script="$script
s#href=\"blas-microbenchmark.html\#$id\"#href=\"$p.html\#$id\"#g"
    done
done
# And the manual's own references to its sections, in bold capitals
# ("see THREADS"), become links to the page that holds them. mandoc may
# break such a name across lines, so these are matched in the whole file.
bold=
for id in $sections; do
    p=$(manual_page_of "$id")
    name=$(echo "$id" | tr _ ' ')
    pattern=$(echo "$id" | sed 's/_/[[:space:]]\\{1,\\}/g')
    bold="$bold
s#<b>$pattern</b>#<a href=\"$p.html\#$id\"><b>$name</b></a>#g"
done
for f in "$out"/*.html; do
    sed -i "$script" "$f"
    sed -i -e ':a' -e 'N' -e '$!ba' -e "$bold" "$f"
done
