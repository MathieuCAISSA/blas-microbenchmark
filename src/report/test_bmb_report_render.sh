#!/bin/sh
# Renders a report in a headless browser and checks that every chart drew.
#
# test_bmb_report.sh checks the command and the data it embeds, but not the
# page's own JavaScript: a typo there would pass it and ship a blank page.
# So this generates input meant to trigger each chart, lets a real browser
# run the page, and looks at the DOM it ends up with.
#
# It needs Chrome or Chromium (for --dump-dom); without one it reports SKIP
# rather than failing. GitHub's x86 runners have Chrome, so CI runs it.
# BMB_BROWSER overrides the search.
set -e

T=render-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

browser=${BMB_BROWSER:-}
if [ -z "$browser" ]; then
    for b in google-chrome google-chrome-stable chromium chromium-browser; do
        if command -v "$b" >/dev/null 2>&1; then
            browser=$b
            break
        fi
    done
fi
if [ -z "$browser" ]; then
    echo "SKIP: no Chrome or Chromium to render the page with"
    exit 77
fi
echo "     rendering with $browser"

rm -rf "$T"
mkdir "$T"

# One input per chart:
#  - ddot over a range: performance and bandwidth against size;
#  - dgemm twice, under two labels so they are two series, at two thread
#    counts: the comparison chart and thread scaling;
#  - dgemv as a grid, at one thread only: the heatmap.
../c/level1/bmb_ddot -x 0 -i 1 -v 8:64 -o "$T/ddot.json" >/dev/null
../c/level3/bmb_dgemm -x 0 -i 1 -m 8:32 -t 1,2 --label a -o "$T/dgemm-a.json" >/dev/null
../c/level3/bmb_dgemm -x 0 -i 1 -m 8:32 -t 1,2 --label b -o "$T/dgemm-b.json" >/dev/null
../c/level2/bmb_dgemv -x 0 -i 1 -m 8:16 -M 8:16 -o "$T/dgemv.json" >/dev/null

./bmb_report "$T"/*.json >"$T/report.html"

# Chrome runs headless without a sandbox here: the page is our own, read
# from disk, and the user-namespace sandbox is often unavailable on CI.
"$browser" --headless=new --no-sandbox --disable-gpu --dump-dom \
    "file://$(pwd)/$T/report.html" >"$T/dom.html" 2>"$T/browser.log" \
    || fail "the browser did not render the page: $(cat "$T/browser.log")"

test -s "$T/dom.html" || fail "the browser returned an empty DOM"

# Only what the page *rendered*, inside <main>. The serialised DOM also
# carries the page's own script, whose source contains every chart title
# and error message verbatim: grepping the whole document would find them
# there and pass even if nothing had been drawn.
dom=$T/main.html
# awk, not a sed range: the page builds <main> in one go, so it opens and
# closes on the same line, and a sed range only looks for its end from the
# *next* line on -- it would run on through the script to the end.
awk '/<main id="bmb-report">/ { on = 1 } on { print } on && /<\/main>/ { exit }' "$T/dom.html" >"$dom"
test -s "$dom" || fail "no <main> element in the rendered page"

# The placeholder is replaced as the very first thing the script does, and
# everything after it builds the page: if it is still there, the script
# died before doing anything.
if grep -q 'Loading the results' "$dom"; then
    fail "the page script did not run"
fi
if grep -q 'could not be read' "$dom"; then
    fail "the page could not read one of the results"
fi

for title in 'Performance against size' 'Bandwidth against size' 'compared with' \
             'Thread scaling' 'Shapes' 'Where the results came from' 'Raw data'; do
    grep -q "$title" "$dom" || fail "no \"$title\" in the rendered page"
    echo "ok   rendered: $title"
done

# At least one drawn line, and heatmap cells: a chart with a title but
# nothing in it would pass the checks above.
grep -q 'class="line"' "$dom" || fail "no line was drawn"
grep -q 'class="cell"' "$dom" || fail "no heatmap cell was drawn"
echo "ok   lines and heatmap cells drawn"

rm -rf "$T"
echo "all checks passed"
