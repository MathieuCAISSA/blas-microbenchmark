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
# Needs mandoc (MANDOC overrides the command). srcdir locates site.css. If
# IMAGES names a directory holding ddot-size-light.png and
# ddot-size-dark.png -- the README's chart, doc/images in a git checkout --
# the index shows it; a release tarball has no such directory, and the
# index goes without.
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
        while (match(text, /<h[12] class="S[hs]" id="[^"]*"><a class="permalink" href="#[^"]*">[^<]*<\/a>/)) {
            h = substr(text, RSTART, RLENGTH)
            text = substr(text, RSTART + RLENGTH)
            id = h; sub(/^.* id="/, "", id); sub(/".*/, "", id)
            name = h; sub(/.*">/, "", name); sub(/<\/a>$/, "", name); gsub(/[ \n]+/, " ", name)
            if (substr(h, 3, 1) == "1") {
                if (sub_open) { print "    </ul>"; sub_open = 0 }
                if (n++) { print "  </li>" }
                printf "  <li class=\"toc-sh\"><a href=\"#%s\">%s</a>\n", id, name
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
    echo "    <a href=\"$REPO\">GitHub</a>"
    echo '  </nav>'
    echo '</div></header>'
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
    echo '</div>'
    if [ -n "$IMAGES" ] && [ -f "$IMAGES/ddot-size-light.png" ] && [ -f "$IMAGES/ddot-size-dark.png" ]; then
        cp "$IMAGES/ddot-size-light.png" "$IMAGES/ddot-size-dark.png" "$out/"
        cat <<EOF
<figure class="shot">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="ddot-size-dark.png"/>
    <img src="ddot-size-light.png" width="1076" height="772"
         alt="A chart from bmb_report: ddot GFLOP/s against vector size for OpenBLAS and BLIS, with the L1, L2 and L3 sizes marked"/>
  </picture>
  <figcaption>A chart from <a href="bmb_report.html">bmb_report</a>: ddot, OpenBLAS against BLIS, with the caches marked.</figcaption>
</figure>
EOF
    fi
    echo "<p class=\"note\">These are the manual pages of blas-microbenchmark $version, also installed with it, built from its <code>main</code> branch.</p>"
    echo '</main>'
    footer_html ""
} >"$out/index.html"
