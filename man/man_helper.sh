# Sourced by the man page tests (test_man_*.sh), from the build directory
# man/, where make has generated the pages.
#
# A tool a test needs but cannot find makes it SKIP, as everywhere else in
# make check -- unless BMB_MAN_STRICT is set, as in the CI job dedicated to
# the man pages, where a SKIP would let the pages go unchecked.

PAGES="blas-microbenchmark.1 bmb_report.1"
BENCH=../src/c/level3/bmb_dgemm
REPORT=../src/report/bmb_report

fail() {
    echo "FAIL: $*"
    exit 1
}

ok() {
    echo "ok   $*"
}

# need TOOL WHY: SKIP the whole test without TOOL.
need() {
    command -v "$1" >/dev/null 2>&1 && return 0
    if [ -n "$BMB_MAN_STRICT" ]; then
        fail "$1 is needed to $2, and BMB_MAN_STRICT is set"
    fi
    echo "SKIP: no $1 to $2"
    exit 77
}

# have TOOL WHY: false without TOOL, so that one check can be left out
# while the others still run.
have() {
    command -v "$1" >/dev/null 2>&1 && return 0
    if [ -n "$BMB_MAN_STRICT" ]; then
        fail "$1 is needed to $2, and BMB_MAN_STRICT is set"
    fi
    echo "--   no $1 to $2: that check is left out"
    return 1
}

# The page's source with roff's \- turned into -, the way a reader sees it.
unescape() {
    sed 's/\\-/-/g' "$1"
}

# The lines of section NAME (.SH NAME up to the next .SH).
section() {
    awk -v name="$2" '$0 == ".SH " name || $0 == ".SH \"" name "\"" { on = 1; next }
                      /^\.SH / { on = 0 }
                      on' "$1"
}

T=$(basename "$0" .sh).d
rm -rf "$T"
mkdir "$T"
for page in $PAGES; do
    test -s "$page" || fail "$page was not generated"
done
