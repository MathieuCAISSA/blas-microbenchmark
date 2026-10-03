#!/bin/sh
# Checks the man pages: that they render without a single groff warning,
# that they document exactly the options --help lists, and that every
# benchmark built has a name in them and an alias page.
#
# The man pages hold the full documentation and --help the summary, so the
# two drift apart easily; this is what notices. Without groff it reports
# SKIP.
set -e

fail() {
    echo "FAIL: $*"
    exit 1
}

if ! command -v groff >/dev/null 2>&1; then
    echo "SKIP: no groff to render the man pages with"
    exit 77
fi

T=man-test.d
rm -rf "$T"
mkdir "$T"

for page in blas-microbenchmark.1 bmb_report.1; do
    test -s "$page" || fail "$page was not generated"
    if grep -q '@[A-Za-z_]*@' "$page"; then
        fail "$page has an unsubstituted placeholder: $(grep -o '@[A-Za-z_]*@' "$page" | head -1)"
    fi
    # All warnings but "cannot break line": the install path is in the
    # page, and a long --prefix (make distcheck's, for one) is no error.
    LC_ALL=C.UTF-8 groff -t -man -Tutf8 -ww -Wbreak -z "$page" 2>"$T/warnings" || true
    if [ -s "$T/warnings" ]; then
        fail "$page renders with warnings: $(cat "$T/warnings")"
    fi
    echo "ok   $page renders without warnings"
done

# "-x, --warmup" pairs: from --help, and from the tags of the OPTIONS
# section of the page (the lines after .TP), with roff's \- undone.
help_options() {
    "$1" --help | grep -oE '^  -[a-zA-Z], --[a-z0-9-]+' | sed 's/^  //' | sort
}
man_options() {
    awk '/^\.SH OPTIONS/ { on = 1; next } /^\.SH / { on = 0 } on && tag { print; tag = 0 } on && /^\.TP/ { tag = 1 }' "$1" \
        | sed 's/\\-/-/g' | grep -oE -- '-[a-zA-Z], --[a-z0-9-]+' | sort
}
same_options() {
    help_options "$1" >"$T/help"
    man_options "$2" >"$T/man"
    test -s "$T/help" || fail "no options found in $1 --help"
    if ! cmp -s "$T/help" "$T/man"; then
        echo "--- $1 --help"
        echo "+++ $2"
        diff "$T/help" "$T/man" || true
        fail "$2 and $1 --help do not list the same options"
    fi
    echo "ok   $2 documents exactly the options of --help ($(wc -l <"$T/help") of them)"
}
same_options ../src/c/level3/bmb_dgemm blas-microbenchmark.1
same_options ../src/report/bmb_report bmb_report.1

# Every benchmark built is named in the page and gets an alias page; every
# alias is a benchmark that exists.
built=$(for f in ../src/c/level1/bmb_* ../src/c/level2/bmb_* ../src/c/level3/bmb_*; do
    n=$(basename "$f")
    case $n in *.*) continue ;; esac
    [ -f "$f" ] && [ -x "$f" ] && printf '%s\n' "${n#bmb_}"
done | sort)
listed=$(printf '%s\n' $ROUTINES | sort)
test "$built" = "$listed" || fail "ROUTINES in man/Makefile.am is not the list of benchmarks built:
built:  $(echo $built)
listed: $(echo $listed)"
names=$(sed -n '/^\.SH NAME/,/^\.SH /p' blas-microbenchmark.1)
for r in $built; do
    printf '%s\n' "$names" | grep -q "bmb_$r[,\\ ]" || fail "bmb_$r is not in the NAME section"
done
echo "ok   every benchmark is named in the page and has an alias ($(echo $built | wc -w) of them)"

# Commands, options and paths must survive a copy and paste, so their
# hyphens are written \-: a plain - renders as a typographic hyphen (U+2010)
# under groff 1.23 on most systems. Debian and Ubuntu map it back to ASCII
# in their man.local, which hides the problem there, so this checks the
# source rather than the rendering: in examples (.EX) and in the font
# macros that set literals, every hyphen must be escaped.
for page in blas-microbenchmark.1 bmb_report.1; do
    awk '/^\.\\"/ { next }
         /^\.EX/ { ex = 1; next }
         /^\.EE/ { ex = 0; next }
         ex || /^\.(B|I|BI|IB|BR|RB|IR|RI|TQ) / {
             line = $0
             gsub(/\\-/, "", line)
             if (line ~ /-/) { print FILENAME ":" NR ": " $0 }
         }' "$page" >"$T/hyphens"
    if [ -s "$T/hyphens" ]; then
        fail "unescaped hyphens, write them as \\-:
$(cat "$T/hyphens")"
    fi
done
echo "ok   every hyphen in a command, option or path is escaped"

rm -rf "$T"
echo "all checks passed"
