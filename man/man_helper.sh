# Sourced by the man page tests (test_man_*.sh), from the build directory
# man/, where make has generated the pages.
#
# A tool a test needs but cannot find makes it SKIP, as everywhere else in
# make check -- unless BMB_MAN_STRICT is set, as in the CI job dedicated to
# the man pages, where a SKIP would let the pages go unchecked.

PAGES="blas-microbenchmark.1 bmb_report.1"
BENCH=../src/c/level3/double/bmb_dgemm
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

# Where the binary of routine $2 is in level $1, or nothing: in the
# build, a level has a directory per precision (single, double, ...).
bin_in_level() {
    for f in ../src/c/level"$1"/*/bmb_"$2"; do
        if [ -f "$f" ] && [ -x "$f" ]; then
            echo "$f"
            return
        fi
    done
}

# Where the binary of a routine is, or nothing.
bin_of() {
    for l in 1 2 3; do
        b=$(bin_in_level "$l" "$1")
        if [ -n "$b" ]; then
            echo "$b"
            return
        fi
    done
}

# A program's options, from its --help: "-x --long" and, for one that
# takes a value, "<" after them.
options_of() {
    "$1" --help | sed -n 's/^  \(-[a-zA-Z]\), \(--[a-z0-9-]*\)\( <\)\{0,1\}.*/\1 \2 \3/p'
}

# check_commands FILE: every command in FILE, one per line, that runs a
# benchmark or bmb_report runs one that exists, with options it takes;
# commands_checked is set to how many it checked. Not in a $(...): there, a fail
# would only end the subshell.
check_commands() {
    commands_checked=0
    while read -r line; do
        # shellcheck disable=SC2086
        set -- $line
        while [ $# -gt 0 ]; do
            case $1 in
                bmb_report) prog=$REPORT ;;
                bmb_*) prog=$(bin_of "${1#bmb_}") ;;
                *) shift; continue ;;
            esac
            [ -n "$prog" ] || fail "$1, in \"$line\", does not exist"
            options_of "$prog" >"$T/opts"
            shift
            while [ $# -gt 0 ]; do
                case $1 in
                    '#' | '>' | '|') break ;;
                    -*)
                        known=$(awk -v o="$1" '$1 == o || $2 == o { print ($3 == "<" ? "arg" : "flag") }' "$T/opts")
                        test -n "$known" || fail "\"$line\" uses $1, which $(basename "$prog") does not take"
                        [ "$known" = arg ] && shift
                        ;;
                esac
                [ $# -gt 0 ] && shift
            done
            commands_checked=$((commands_checked + 1))
            break
        done
    done <"$1"
}

T=$(basename "$0" .sh).d
rm -rf "$T"
mkdir "$T"
for page in $PAGES; do
    test -s "$page" || fail "$page was not generated"
done
