#!/bin/sh
# Checks that the man pages document exactly the options --help lists, and
# give the same defaults. --help is the summary and the man page the
# reference; this is what notices when one moves without the other.
set -e
. "${srcdir:-.}/man_helper.sh"

# "-x, --warmup", one per line, from --help...
help_options() {
    "$1" --help | grep -oE '^  -[a-zA-Z], --[a-z0-9-]+' | sed 's/^  //' | sort
}
# ...and from the tags of the page's OPTIONS section (the line after .TP).
man_options() {
    section "$1" OPTIONS | sed 's/\\-/-/g' \
        | awk 'tag { print; tag = 0 } /^\.TP/ { tag = 1 }' \
        | grep -oE -- '-[a-zA-Z], --[a-z0-9-]+' | sort
}

# "-x, --warmup=1" for every option whose default is a number: from --help,
# where it reads "(default: 1", possibly on a continuation line...
help_defaults() {
    "$1" --help | awk '
        /^  -[a-zA-Z], --/ { flush(); match($0, /-[a-zA-Z], --[a-z0-9-]+/); key = substr($0, RSTART, RLENGTH); text = $0; next }
        /^      / && key != "" { text = text " " $0; next }
        { flush() }
        END { flush() }
        function flush() {
            if (key != "" && match(text, /\(default: [0-9]+/)) {
                print key "=" substr(text, RSTART + 10, RLENGTH - 10)
            }
            key = ""
        }' | sort
}
# ...and from the page, where the option's paragraph says "Default: 1."
man_defaults() {
    section "$1" OPTIONS | sed 's/\\-/-/g' | awk '
        /^\.TP/ { flush(); tag = 1; next }
        tag { match($0, /-[a-zA-Z], --[a-z0-9-]+/); key = substr($0, RSTART, RLENGTH); tag = 0; text = ""; next }
        /^\.S[SH]/ { flush(); next }
        { text = text " " $0 }
        END { flush() }
        function flush() {
            if (key != "" && match(text, /Default: [0-9]+/)) {
                print key "=" substr(text, RSTART + 9, RLENGTH - 9)
            }
            key = ""
        }' | sort
}

compare() {
    what=$1 prog=$2 page=$3
    "help_$what" "$prog" >"$T/help"
    "man_$what" "$page" >"$T/man"
    if ! cmp -s "$T/help" "$T/man"; then
        echo "--- $prog --help"
        echo "+++ $page"
        diff "$T/help" "$T/man" || true
        fail "$page and $prog --help do not give the same $what"
    fi
}

compare options "$BENCH" blas-microbenchmark.1
test "$(wc -l <"$T/help")" -ge 13 || fail "only $(wc -l <"$T/help") options read from $BENCH --help"
ok "blas-microbenchmark.1 documents exactly the $(wc -l <"$T/help") options of --help"

compare defaults "$BENCH" blas-microbenchmark.1
test "$(wc -l <"$T/help")" -ge 6 || fail "only $(wc -l <"$T/help") numeric defaults read from $BENCH --help"
ok "blas-microbenchmark.1 gives the same $(wc -l <"$T/help") numeric defaults as --help"

compare options "$REPORT" bmb_report.1
ok "bmb_report.1 documents exactly the $(wc -l <"$T/help") options of --help"

# The benchmarks share one parser, so one of them speaks for all; but the
# page says so, and that is worth checking too.
for b in ../src/c/level1/bmb_ddot ../src/c/level2/bmb_dgemv; do
    "$b" --help | sed 1d >"$T/other"
    "$BENCH" --help | sed 1d >"$T/ref"
    cmp -s "$T/other" "$T/ref" || fail "$b --help differs from $BENCH --help, but the page says they share their options"
done
ok "the benchmarks all take the same options, as the page says"

# Both --help texts point at their man page.
"$BENCH" --help | grep -q 'man blas-microbenchmark' || fail "$BENCH --help does not point at the man page"
"$REPORT" --help | grep -q 'man bmb_report' || fail "$REPORT --help does not point at the man page"
ok "--help points at the man page"

rm -rf "$T"
echo "all checks passed"
