#!/bin/sh
# Checks assemble.awk, which builds bmb_report by inserting report.html into
# bmb_report.sh: that it copies the template byte for byte, and that it
# refuses to build in each case where the result would be broken.
#
# Run with the awk configure found (AWK, from the Makefile), since that is
# the one the build uses -- gawk on some systems, mawk on others.
set -e

AWK=${AWK:-awk}
S=${srcdir:-.}
T=assemble-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

rm -rf "$T"
mkdir "$T"

# A template exercising everything a shell would otherwise expand.
cat >"$T/page.html" <<'EOF'
<p>$HOME `id` $(date) \n "quoted" 'single' @PACKAGE_VERSION@</p>
<!-- @BMB_RESULTS@ -->
<p>tail</p>
EOF

"$AWK" -v version=1.2.3 -f "$S/assemble.awk" "$T/page.html" "$S/bmb_report.sh" >"$T/bmb_report" \
    || fail "a valid template did not assemble"
sh -n "$T/bmb_report" || fail "the assembled script is not valid sh"
echo "ok   a valid template assembles into valid sh"

# The script's own placeholders are substituted; the template's are not.
grep -q "^BMB_VERSION='1.2.3'$" "$T/bmb_report" || fail "@PACKAGE_VERSION@ not substituted in the script"
grep -q "^BMB_ACCEPT_MAJOR='1'$" "$T/bmb_report" || fail "the major version was not taken from 1.2.3"
echo "ok   the version and its major are substituted into the script"

# Run it: the template must come out exactly as written -- no $HOME, no
# command substitution, no backslash handling, and its @PACKAGE_VERSION@
# left alone, since the template is not the script.
printf '{\n  "version": "1.2.3",\n  "results": [\n  ]\n}\n' >"$T/r.json"
sh "$T/bmb_report" "$T/r.json" >"$T/out.html" || fail "the assembled script did not run"
grep -qF '<p>$HOME `id` $(date) \n "quoted" '"'"'single'"'"' @PACKAGE_VERSION@</p>' "$T/out.html" \
    || fail "the template was not copied byte for byte: $(sed -n 1p "$T/out.html")"
grep -qF '<p>tail</p>' "$T/out.html" || fail "the part after the marker is missing"
echo "ok   the template is copied byte for byte, with nothing expanded"

refuse() {
    if "$AWK" -v version="$2" -f "$S/assemble.awk" "$1" "$S/bmb_report.sh" >"$T/out" 2>"$T/err"; then
        fail "assembled although $3"
    fi
    grep -q "$4" "$T/err" || fail "$3: the message does not say '$4': $(cat "$T/err")"
    echo "ok   refuses to build when $3"
}

printf '<p>no marker</p>\n' >"$T/nomarker.html"
refuse "$T/nomarker.html" 1.0.0 "the results marker is missing" "has no <!-- @BMB_RESULTS@ --> line"

printf '<!-- @BMB_RESULTS@ -->\n<!-- @BMB_RESULTS@ -->\n' >"$T/twice.html"
refuse "$T/twice.html" 1.0.0 "the results marker appears twice" "marker twice"

# A line equal to a here-document delimiter would end it early and leave
# the rest of the page to be run as shell.
printf 'BMB_REPORT_HEAD\n<!-- @BMB_RESULTS@ -->\n' >"$T/head.html"
refuse "$T/head.html" 1.0.0 "the template contains BMB_REPORT_HEAD" "would end a here-document early"
printf '<!-- @BMB_RESULTS@ -->\nBMB_REPORT_TAIL\n' >"$T/tail.html"
refuse "$T/tail.html" 1.0.0 "the template contains BMB_REPORT_TAIL" "would end a here-document early"

refuse "$S/report.html" dev "the version has no numeric major" "cannot take a major version"

rm -rf "$T"
echo "all checks passed"
