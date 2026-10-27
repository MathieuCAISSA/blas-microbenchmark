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
../c/level1/double/bmb_ddot -x 0 -i 1 -v 8:32 -o "$T/ddot.json" >/dev/null
../c/level3/double/bmb_dgemm -x 0 -i 1 -m 8:16 --label 'x</script><b>y' -o "$T/dgemm.json" >/dev/null

# ---- usage ----
if $R >"$T/out" 2>"$T/err"; then
    fail "running with no argument succeeded"
fi
if [ -s "$T/out" ]; then
    fail "the usage message went to stdout"
fi
pass "no argument is a usage error"

# The build's version, not merely a version: a bmb_report not rebuilt
# after configure changed the version kept the old one (#60).
v=$($R --version)
test "$v" = "bmb_report ${PACKAGE_VERSION:?PACKAGE_VERSION is not set}" \
    || fail "--version says '$v', this build is $PACKAGE_VERSION"
pass "--version gives this build's version"

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

# ---- --peak-gflops and --peak-gbs (#251) ----
# A peak is a plain decimal above 0 and up to 1000000, as the benchmarks
# take it; anything else is refused before any page is written. A taken
# one reaches the page as a JSON number: 007 as 7, which JSON can read.
for bad in 0 0.0 -1 1e3 inf .5 5. 1000000.5 32x ''; do
    for opt in --peak-gflops --peak-gbs; do
        if $R "$opt" "$bad" "$T/ddot.json" >"$T/out" 2>"$T/err"; then
            fail "$opt '$bad' was accepted"
        fi
        test -s "$T/out" && fail "$opt '$bad': a page was written"
        grep -q -- "$opt takes a number above 0" "$T/err" || fail "$opt '$bad': $(cat "$T/err")"
    done
done
if $R --peak-gbs >"$T/out" 2>"$T/err"; then
    fail "--peak-gbs without a value was accepted"
fi
grep -q 'needs a value' "$T/err" || fail "--peak-gbs without a value: $(cat "$T/err")"
pass "refuses a peak that is not a number above 0 and up to 1000000, or none"
$R --peak-gflops 007 --peak-gbs=204.8 "$T/ddot.json" >"$T/peak.html" 2>"$T/err" ||
    fail "a valid peak was refused: $(cat "$T/err")"
grep -q '<script type="application/json" id="bmb-meta">{.*, "peak_gflops": 7, "peak_gbs": 204.8}' "$T/peak.html" ||
    fail "the page does not carry the peak given: $(grep 'bmb-meta' "$T/peak.html")"
pass "a peak given, in the page as a JSON number, by either spelling of the option"

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

# ---- a directory (#253) ----
# Every *.json under it, at any depth, in name order whatever order they
# were written in; anything else beside them left alone. A directory and
# a file mix.
mkdir -p "$T/dir/b" "$T/dir/a/deep" "$T/empty/sub"
cp "$T/ddot.json" "$T/dir/b/ddot.json"
cp "$T/dgemm.json" "$T/dir/b/a dgemm.json"
cp "$T/ddot.json" "$T/dir/a/deep/ddot.json"
cp "$T/dgemm.json" "$T/dir/a/dgemm.json"
echo 'a log' >"$T/dir/a/dgemm.log"
$R "$T/dir" "$T/ddot.json" >"$T/dir.html" 2>"$T/err" || fail "a directory was refused: $(cat "$T/err")"
sed -n 's/^<script type="application\/json" class="bmb-result">{"file": "\([^"]*\)", "result":$/\1/p' \
    "$T/dir.html" >"$T/files"
printf '%s\n' "$T/dir/a/deep/ddot.json" "$T/dir/a/dgemm.json" "$T/dir/b/a dgemm.json" \
    "$T/dir/b/ddot.json" "$T/ddot.json" >"$T/expected"
cmp -s "$T/files" "$T/expected" || fail "a directory and a file did not give their results in order:
$(diff "$T/expected" "$T/files")"
pass "a directory: every result under it, at any depth, in name order, then a file"

# refuse_dir DIR MESSAGE NAME WHAT: DIR refused, no page written, with
# MESSAGE naming NAME; WHAT is what the case is.
refuse_dir() {
    if $R "$T/ddot.json" "$1" >"$T/out" 2>"$T/err"; then
        fail "$1 was accepted"
    fi
    test -s "$T/out" && fail "$1: a page was written although it was refused"
    grep -q "$2" "$T/err" || fail "$1: the message does not say '$2': $(cat "$T/err")"
    grep -qF "$3" "$T/err" || fail "$1: the message does not name $3: $(cat "$T/err")"
    pass "refuses $4"
}
refuse_dir "$T/empty" "no result" "$T/empty" "a directory without a result"
cp "$F/not-ours.json" "$T/dir/b/c.json"
refuse_dir "$T/dir" "not a blas-microbenchmark result" "$T/dir/b/c.json" \
    "a directory holding a file that is not a result"
rm "$T/dir/b/c.json"
cp "$T/ddot.json" "$nl_name"
mv "$nl_name" "$T/dir/a/"
refuse_dir "$T/dir" "contains a newline" "$T/dir" "a directory holding a file name with a newline"

# ---- the page knows every routine's level ----
# Its LEVEL table orders the sections and names each routine's level; a
# routine missing from it would sort first, with no level, and no other
# test would notice (#52). Against the benchmarks built, both ways.
sed -n '/^  var LEVEL = {$/,/^  };$/p' "${srcdir:-.}/report.html" \
    | grep -o '[a-z0-9]*: [123]' | sort >"$T/levels"
test -s "$T/levels" || fail "no LEVEL table found in report.html"
for f in ../c/level1/*/bmb_* ../c/level2/*/bmb_* ../c/level3/*/bmb_*; do
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
