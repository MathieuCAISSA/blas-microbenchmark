#!/bin/sh
# Checks bmb_report: what it refuses, and that what it writes is one
# self-contained page carrying every result.
set -e

R=./bmb_report
F=${srcdir:-.}/fixtures
T=report-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

pass() {
    echo "ok   $*"
}

rm -rf "$T"
mkdir "$T"

# Current-format input is generated, not kept as a fixture, so it can never
# fall out of date. The dgemm label carries a "</script>" on purpose.
../c/level1/bmb_ddot -x 0 -i 1 -v 8:32 -o "$T/ddot.json" >/dev/null
../c/level3/bmb_dgemm -x 0 -i 1 -m 8:16 --label 'x</script><b>y' -o "$T/dgemm.json" >/dev/null

# ---- usage ----
if $R >"$T/out" 2>"$T/err"; then
    fail "running with no argument succeeded"
fi
if [ -s "$T/out" ]; then
    fail "the usage message went to stdout"
fi
pass "no argument is a usage error"

$R --version | grep -q '^bmb_report [0-9]' || fail "--version"
pass "--version"

# ---- refusals (#1, decision 5) ----
# Each one names the file and the reason. Each is given next to a valid
# file, because one refused file must mean no page at all.
refuse() {
    if $R "$T/ddot.json" "$1" >"$T/out" 2>"$T/err"; then
        fail "$1 was accepted"
    fi
    if [ -s "$T/out" ]; then
        fail "$1: a page was written although a file was refused"
    fi
    grep -q "$2" "$T/err" || fail "$1: the message does not say '$2': $(cat "$T/err")"
    grep -qF "$1" "$T/err" || fail "$1: the message does not name the file"
    pass "refuses $(basename "$1") ($2)"
}

refuse "$F/v0.5.0.json" "before 0.6.0"
refuse "$F/v0.6.1.json" "records neither the"
refuse "$F/v2.0.0.json" "Update blas-microbenchmark"
refuse "$F/not-ours.json" "not a blas-microbenchmark result"
sed '$d' "$T/ddot.json" >"$T/truncated.json"
refuse "$T/truncated.json" "incomplete"
refuse "$T/missing.json" "cannot be read"

# A newline in a file name would end up inside the JSON string that names
# the file in the page, which JSON does not allow.
nl_name="$T/two
lines.json"
cp "$T/ddot.json" "$nl_name"
if $R "$T/ddot.json" "$nl_name" >"$T/out" 2>"$T/err"; then
    fail "a file name containing a newline was accepted"
fi
grep -q "contains a newline" "$T/err" || fail "the newline refusal does not say why: $(cat "$T/err")"
pass "refuses a file name containing a newline"

# ---- the page ----
$R "$T/ddot.json" "$T/dgemm.json" >"$T/report.html" 2>"$T/err" \
    || fail "valid input was refused: $(cat "$T/err")"
grep -qi '^<!doctype html>' "$T/report.html" || fail "the output is not an HTML page"
test "$(grep -c 'class="bmb-result"' "$T/report.html")" -eq 2 \
    || fail "not exactly one data block per file"
pass "one page, one data block per file"

# Self-contained: it has to open from file:// with no network.
if grep -Eiq "(src|href)[[:space:]]*=[[:space:]]*[\"']?(https?:)?//" "$T/report.html"; then
    fail "the page references something on the network"
fi
if grep -qi '<script[^>]*src=' "$T/report.html"; then
    fail "the page loads an external script"
fi
if grep -qi '<link[^>]*stylesheet' "$T/report.html"; then
    fail "the page loads an external stylesheet"
fi
pass "self-contained: no external script, stylesheet or URL"

# A "</script>" in the data must stay inside its block, not end it.
opened=$(grep -o '<script' "$T/report.html" | wc -l)
closed=$(grep -o '</script>' "$T/report.html" | wc -l)
test "$opened" -eq "$closed" \
    || fail "$opened <script> for $closed </script>: something in the data closed an element"
grep -qF 'x<\/script><b>y' "$T/report.html" || fail "the label was not escaped"
pass "a </script> inside a label stays inside its data block"

# ---- the page knows every routine's level ----
# Its LEVEL table orders the sections and names each routine's level; a
# routine missing from it would sort first, with no level, and no other
# test would notice (#52). Against the benchmarks built, both ways.
sed -n '/^  var LEVEL = {$/,/^  };$/p' "${srcdir:-.}/report.html" \
    | grep -o '[a-z0-9]*: [123]' | sort >"$T/levels"
test -s "$T/levels" || fail "no LEVEL table found in report.html"
for f in ../c/level1/bmb_* ../c/level2/bmb_* ../c/level3/bmb_*; do
    n=$(basename "$f")
    case $n in *.*) continue ;; esac
    [ -f "$f" ] && [ -x "$f" ] || continue
    l=${f#../c/level}
    printf '%s: %s\n' "${n#bmb_}" "${l%%/*}"
done | sort >"$T/built"
test -s "$T/built" || fail "no benchmark found in ../c/level*"
cmp -s "$T/levels" "$T/built" || fail "report.html's LEVEL table is not the benchmarks built:
$(diff "$T/built" "$T/levels")"
pass "the page's LEVEL table lists every benchmark built, in its level ($(wc -l <"$T/built"))"

rm -rf "$T"
echo "all checks passed"
