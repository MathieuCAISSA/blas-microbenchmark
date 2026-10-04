#!/bin/sh
# Builds the documentation site from the man pages:
#
#     html.sh OUTDIR VERSION PAGE.1...
#
# writes OUTDIR/<page>.html for each page, an index.html listing them, and
# mandoc.css. `make html` runs it; the Pages workflow publishes the result.
#
# The pages are the man pages and nothing else -- the same source, checked
# by the same tests -- so the site cannot say anything the man pages do
# not. mandoc does the conversion. What it leaves undone for man(7) pages
# is done here: a reference such as bmb_report(1) becomes a link, to the
# page on this site when there is one and to man7.org otherwise, and so
# does a URL in the text. (The pages could mark URLs with .UR, but groff
# then shows only the link text in a terminal, and the URL is lost.)
#
# Needs mandoc (MANDOC overrides the command). srcdir locates mandoc.css,
# the stylesheet mandoc ships, and site.css, appended to it.
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

rm -rf "$out"
mkdir -p "$out"
cat "${srcdir:-.}/mandoc.css" "${srcdir:-.}/site.css" >"$out/mandoc.css"

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

items=
for page; do
    name=$(basename "$page" .1)
    # Through a file, not a pipe: in a pipe, a mandoc failure would be
    # hidden behind sed's success, and leave an empty page.
    "$MANDOC" -Thtml -Ostyle=mandoc.css "$page" >"$out/$name.tmp"
    test -s "$out/$name.tmp" || { echo "$0: mandoc produced nothing for $page" >&2; exit 1; }
    link_references <"$out/$name.tmp" >"$out/$name.html"
    rm -f "$out/$name.tmp"
    items="$items    <li><a href=\"$name.html\"><b>$name</b>(1)</a> &mdash; $(description "$page")</li>
"
done

cat >"$out/index.html" <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <link rel="stylesheet" href="mandoc.css" type="text/css" media="all"/>
  <title>blas-microbenchmark $version</title>
</head>
<body>
<main class="manual-text">
  <h1 class="Sh">blas-microbenchmark $version</h1>
  <p class="Pp">Command-line microbenchmarks for BLAS routines. These are its
    manual pages, also installed with it (<code>man blas-microbenchmark</code>,
    <code>man bmb_report</code>):</p>
  <ul>
$items  </ul>
  <p class="Pp">Installing it, and the source:
    <a href="https://github.com/MathieuCAISSA/blas-microbenchmark">github.com/MathieuCAISSA/blas-microbenchmark</a>.
    These pages are built from its <code>main</code> branch.</p>
</main>
</body>
</html>
EOF
