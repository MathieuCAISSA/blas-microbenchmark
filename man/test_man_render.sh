#!/bin/sh
# Checks that the man pages render cleanly: every placeholder substituted,
# a date, no groff warning, nothing mandoc's lint objects to, and no
# command, option or path that would come out as a typographic hyphen.
set -e
. "${srcdir:-.}/man_helper.sh"
need groff "render the man pages"

for page in $PAGES; do
    if grep -q '@[A-Za-z_]*@' "$page"; then
        fail "$page has an unsubstituted placeholder: $(grep -o '@[A-Za-z_]*@' "$page" | head -1)"
    fi
    date=$(awk '/^\.TH / { print $4; exit }' "$page")
    printf '%s\n' "$date" | grep -qE '^[0-9]{4}-[0-9]{2}-[0-9]{2}$' \
        || fail "$page: no date (YYYY-MM-DD) in its .TH line"
done
ok "placeholders substituted, pages dated"

# Every warning but "cannot break line": the install path is in the page,
# and a long --prefix (make distcheck's, for one) is no error.
for page in $PAGES; do
    LC_ALL=C.UTF-8 groff -t -man -Tutf8 -ww -Wbreak -z "$page" 2>"$T/warnings" || true
    if [ -s "$T/warnings" ]; then
        fail "$page renders with warnings: $(cat "$T/warnings")"
    fi
done
ok "groff renders both pages without a warning"

# mandoc is another implementation, and a stricter one: what it accepts
# renders the same under man-db, mandoc and the BSDs.
if have mandoc "lint the pages"; then
    for page in $PAGES; do
        mandoc -Tlint -Wall "$page" >"$T/lint" 2>&1 || fail "mandoc -Tlint on $page: $(cat "$T/lint")"
    done
    ok "mandoc -Tlint -Wall has nothing to say"
fi

# Commands, options and paths must survive a copy and paste, so their
# hyphens are written \-: a plain - renders as U+2010 under groff 1.23,
# which no shell takes for a hyphen. In examples (.EX) and in the font
# macros that set literals, every hyphen must be escaped.
for page in $PAGES; do
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
ok "every hyphen in a command, option or path is escaped in the source"

# The same, in what a reader gets. Debian and Ubuntu map - back to ASCII
# in their man.local, which would hide the problem here, so the pages are
# rendered with an empty man.local first in the macro path -- as groff
# itself, and Fedora, Arch or macOS, render them. A U+2010 at the start of
# a word is an option; one after a / is in a path. (Hyphenated prose,
# "single-threaded", is meant to get one.)
mkdir "$T/tmac"
: >"$T/tmac/man.local"
hy=$(printf '\342\200\220')
for page in $PAGES; do
    LC_ALL=C.UTF-8 groff -M "$T/tmac" -t -man -Tutf8 -P-cbou "$page" 2>/dev/null >"$T/rendered"
    if LC_ALL=C grep -nE "(^|[ /(=])$hy|/[^ ]*$hy" "$T/rendered" >"$T/bad"; then
        fail "$page renders an option or a path with a typographic hyphen:
$(cat "$T/bad")"
    fi
done
ok "no option or path renders with a typographic hyphen, even without Debian's man.local"

rm -rf "$T"
echo "all checks passed"
