#!/bin/sh
# Renders a report in a headless browser and checks that every chart drew.
#
# test_bmb_report.sh checks the command and the data it embeds, but not the
# page's own JavaScript: a typo there would pass it and ship a blank page.
# So this generates input meant to trigger each chart, lets a real browser
# run the page, and looks at the DOM it ends up with.
#
# It needs a browser (see browser.sh); without one it reports SKIP rather
# than failing. CI runs it in Chrome, Firefox and Edge.
set -e

T=render-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

. "${srcdir:-.}/browser.sh"
find_browser

rm -rf "$T"
mkdir "$T"

# One input per chart:
#  - ddot over a range: performance and bandwidth against size;
#  - dgemm twice, under two labels so they are two series, at two thread
#    counts: the comparison chart and thread scaling; one of them with
#    --verify and the other with -C, so the Verified columns show both,
#    and the other on the fake sysfs of the machine probe's tests, so the
#    Frequency column shows;
#  - dgemm four more times under a third label, so that series has
#    repeated runs: their band, and the noise test in the comparison;
#  - dgemm under a fourth label, as three versions of the library (one
#    run, its version string rewritten, since one version is installed):
#    the history;
#  - dgemv as a grid, at one thread only: the heatmap; with a peak and
#    a bandwidth, its roofline (#251), as dgemm a's peak gives the level 3
#    peak line.
../c/level1/double/bmb_ddot -x 0 -i 1 -v 8:64 -o "$T/ddot.json" >/dev/null
../c/level3/double/bmb_dgemm -x 0 -i 1 -m 8:32 -t 1,2 --label a -c --peak-gflops 16 -o "$T/dgemm-a.json" >/dev/null
BMB_MACHINE_ROOT=${srcdir:-.}/../c/common/fixtures/machine/x86 \
    ../c/level3/double/bmb_dgemm -x 0 -i 1 -m 8:32 -t 1,2 --label b -C -o "$T/dgemm-b.json" >/dev/null
for i in 1 2 3 4; do
    ../c/level3/double/bmb_dgemm -x 0 -i 1 -m 8:32 -t 1,2 --label r -C -o "$T/dgemm-r$i.json" >/dev/null
done
../c/level3/double/bmb_dgemm -x 0 -i 1 -m 8:32 -t 1,2 --label h -o "$T/hist.json" >/dev/null
for v in 0.3.9 0.3.26 0.3.28; do
    # Netlib, NVPL and ArmPL record no version string: add one.
    sed -e '/^  "blas": /d' -e '/^  "backend": /a\
  "blas": "OpenBLAS '"$v"' Haswell",' "$T/hist.json" >"$T/hist-$v.json"
done
rm "$T/hist.json"
../c/level2/double/bmb_dgemv -x 0 -i 1 -m 8:16 -M 8:16 --peak-gflops 16 --peak-gbs 20 -o "$T/dgemv.json" >/dev/null

./bmb_report "$T"/*.json >"$T/report.html"

render_dom "$T/report.html" "$T/dom.html"

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

for title in 'Performance against size' 'Bandwidth against size' '[Cc]ompared with' \
             'Thread scaling' 'Shapes' 'Where the results came from' 'Raw data'; do
    grep -q "$title" "$dom" || fail "no \"$title\" in the rendered page"
    echo "ok   rendered: $title"
done

# At least one drawn line, and heatmap cells: a chart with a title but
# nothing in it would pass the checks above.
# One series verified, the others not: the provenance table and the raw
# data both get a Verified column, saying yes for that series.
test "$(grep -o '>Verified<' "$dom" | wc -l)" -ge 2 || fail "no Verified column in the provenance and raw data tables"
grep -q '<td>yes</td>' "$dom" || fail "the verified series is not shown as verified"
grep -q '<td>no</td>' "$dom" || fail "the series run with -C is not shown as unverified"
echo "ok   rendered: the Verified columns, for the series run with --verify"

grep -q 'version by version' "$dom" || fail "no history for the library measured as three versions"
grep -q '>0.3.9<' "$dom" && grep -q '>0.3.28<' "$dom" || fail "the history's axis does not name the versions"
echo "ok   rendered: the history of a library measured as three versions"

grep -q 'band: fastest to slowest run' "$dom" || fail "no run band for the series measured four times"
grep -q 'Mann-Whitney U' "$dom" || fail "the comparison chart does not say how its points were tested against noise"
rows=$(cat "$T"/*.json | grep -c '"thread_count"')
grep -q "Raw data ($rows rows)" "$dom" || fail "the raw data table does not have a row for each of the $rows runs"
echo "ok   rendered: repeated runs, their band, the noise test, and every run in the raw data"

grep -q '>Frequency<' "$dom" || fail "no Frequency column in the provenance table"
grep -q '<td>performance, turbo off</td>' "$dom" || fail "the series run on the x86 fixture shows no frequency"
echo "ok   rendered: the Frequency column, for the series that recorded one"

grep -q '>Run<' "$dom" || fail "no Run column in the provenance table"
grep -q '<td[^>]*>1 sample, 0.2 s and 0 warm-up calls · ' "$dom" || fail "the Run column does not say how the series were measured"
grep -q 'band: fastest sample to third quartile' "$dom" || fail "no quartile band for the series measured once"
echo "ok   rendered: the Run column, and the samples' band, for files with a run block"

# The peak (#251): dgemv's roofline, with its two roofs and a dot a size;
# dgemm a's peak line; each point's percentage of it in the raw data; and
# where each peak came from.
grep -q '<h3>Roofline</h3>' "$dom" || fail "no roofline for dgemv, given a peak and a bandwidth"
test "$(grep -o 'class="roof"' "$dom" | wc -l)" -ge 2 || fail "the roofline or the level 3 chart draws no roof"
grep -q 'memory roof 20.0 GB/s, compute roof 16.0 GFLOP/s' "$dom" || fail "the roofline does not give its roofs"
# The roof bends at the ridge, where the memory roof meets the compute
# roof: three points, not a slope straight to the end.
grep -E -q '<path d="M[^"L]*L[^"L]*L[^"L]*"[^>]*class="roof"' "$dom" || fail "the roofline's roof does not bend at the ridge"
grep -q '% of the roof</title>' "$dom" || fail "the roofline's dots do not say how far they are from the roof"
# At the thread count the page opens on: a core's 16 times that.
grep -E -q 'peak, (1 core: 16.0|2 cores: 32.0) GFLOP/s' "$dom" || fail "no peak line on dgemm a's performance chart"
grep -q '>% of peak<' "$dom" || fail "no % of peak column in the raw data"
grep -q 'Peak: 16.0 GFLOP/s a core, given in the results' "$dom" || fail "the page does not say where the peak came from"
echo "ok   rendered: the roofline, the peak line, the percentages, and where the peak came from"

# The kernels the library chose (#162), beside it, where it says them.
if grep -q '^  "kernels": ' "$T/ddot.json"; then
    grep -q ' · kernels ' "$dom" || fail "the library's kernels are not shown beside it"
    echo "ok   rendered: the kernels the library chose"
else
    echo "note this library does not say which kernels it chose: nothing to show"
fi

grep -q 'class="line"' "$dom" || fail "no line was drawn"
grep -q 'class="cell"' "$dom" || fail "no heatmap cell was drawn"
echo "ok   lines and heatmap cells drawn"

rm -rf "$T"
echo "all checks passed"
