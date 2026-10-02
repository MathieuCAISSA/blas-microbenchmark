#!/bin/sh
# Runs the unit tests of the page's logic (test_report.js) in a headless
# browser.
#
# The tests go into a real report, after the page's own script, so they
# exercise exactly the code bmb_report ships, through the functions it
# exposes as window.bmbReport. They write their outcome into the page; this
# reads it back from the rendered DOM. Without a browser it reports SKIP --
# see browser.sh for which ones it looks for.
set -e

S=${srcdir:-.}
T=report-js-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

. "$S/browser.sh"
find_browser

rm -rf "$T"
mkdir "$T"

# Pasted inside a <script> element, "</" could end it early.
if grep -n '</' "$S/test_report.js"; then
    fail "test_report.js contains '</', which would end the <script> it is pasted into"
fi

# Any result will do: the tests build their own input. This one is only
# there because bmb_report needs a file.
../c/level1/bmb_ddot -x 0 -i 1 -v 8 -o "$T/ddot.json" >/dev/null
./bmb_report "$T/ddot.json" >"$T/report.html"

grep -q '^</body>$' "$T/report.html" || fail "no </body> line to put the tests before"
awk -v tests="$S/test_report.js" '
    $0 == "</body>" {
        print "<script>"
        while ((getline line < tests) > 0) { print line }
        print "</script>"
    }
    { print }' "$T/report.html" >"$T/test.html"

render_dom "$T/test.html" "$T/dom.html"

# The text of <pre id="bmb-test-results">: the opening tag shares a line
# with the first check (and with whatever precedes it), and the DOM escapes
# & < > in text. Carriage returns
# go, for a browser that ends its lines the Windows way.
tr -d '\r' <"$T/dom.html" \
    | awk '/<pre id="bmb-test-results">/ { on = 1 } on { print } on && /<\/pre>/ { exit }' \
    | sed -e 's/.*<pre id="bmb-test-results">//' -e 's#</pre>.*##' \
          -e 's/&lt;/</g' -e 's/&gt;/>/g' -e 's/&amp;/\&/g' >"$T/results"
if [ ! -s "$T/results" ]; then
    fail "the tests did not run: no results in the page (a syntax error in test_report.js?)"
fi
cat "$T/results"

if grep -q '^FAIL' "$T/results"; then
    exit 1
fi
grep -q '^all [0-9]* checks passed$' "$T/results" || fail "the tests did not finish"

rm -rf "$T"
