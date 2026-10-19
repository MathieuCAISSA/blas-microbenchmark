#!/bin/sh
# Checks the C code's coverage exclusions: each gcovr marker
# (GCOVR_EXCL_LINE, _BR_LINE, _START, _BR_START) says on the same line,
# after a colon, why no test can reach what it excludes, in a few words
# at least; every START has its STOP. An exclusion is for a line that
# cannot run on Linux (a call that does not fail, a clock that does not
# stop), never for one nobody wrote a test for (doc/dev/ci.md,
# Coverage).
set -e

S=${srcdir:-.}

fail() {
    echo "FAIL: $*"
    exit 1
}

files=$(find "$S/src" -name '*.c' | sort)
test -n "$files" || fail "no C file under $S/src"

n=0
bad=0
for f in $files; do
    name=${f#"$S"/}
    # Each marker but STOP: a colon, then a reason of four words or more.
    out=$(awk -v name="$name" '
        /GCOVR_EXCL_(LINE|BR_LINE|START|BR_START)/ {
            n++
            s = $0
            sub(/.*GCOVR_EXCL_(LINE|BR_LINE|START|BR_START)/, "", s)
            sub(/\*\/.*/, "", s)
            if (s !~ /^: / || split(substr(s, 3), w, /[ \t]+/) < 4) {
                printf "FAIL: %s:%d: an exclusion without its reason\n", name, NR
                bad++
            }
        }
        /GCOVR_EXCL_(BR_)?START/ { if (open) { printf "FAIL: %s:%d: GCOVR_EXCL_START inside another\n", name, NR; bad++ } open = NR }
        /GCOVR_EXCL_(BR_)?STOP/ { if (!open) { printf "FAIL: %s:%d: GCOVR_EXCL_STOP without a START\n", name, NR; bad++ } open = 0 }
        END {
            if (open) { printf "FAIL: %s:%d: GCOVR_EXCL_START never stopped\n", name, open; bad++ }
            printf "count %d %d\n", n, bad
        }' "$f")
    echo "$out" | grep -v '^count ' || true
    set -- $(echo "$out" | sed -n 's/^count //p')
    n=$((n + $1))
    bad=$((bad + $2))
done

test "$bad" -eq 0 || fail "$bad exclusion(s) above"
echo "ok   all $n coverage exclusions give their reason, and every START has its STOP"
echo "all checks passed"
