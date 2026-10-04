#!/bin/sh
# Checks that every action the workflows use is pinned to a full commit
# SHA, with its version in a comment (actions/checkout@<40 hex> # v7.0.1).
# A tag can be moved to other code, by its owner or by whoever takes over
# the repository; a commit cannot. Dependabot updates the SHA and the
# comment together.
#
# .github/ is not in the tarball, so this SKIPs there.
set -e

S=${srcdir:-.}

fail() {
    echo "FAIL: $*"
    exit 1
}

test -d "$S/.github/workflows" || { echo "SKIP: no .github/workflows in $S"; exit 77; }

n=0
bad=0
for f in "$S"/.github/workflows/*.yml; do
    name=.github/workflows/$(basename "$f")
    lines=$(grep -n 'uses:' "$f" || true)
    [ -n "$lines" ] || continue
    while IFS= read -r l; do
        n=$((n + 1))
        ref=$(printf '%s\n' "$l" | sed 's/^[0-9]*: *-\{0,1\} *uses: *//')
        if ! printf '%s\n' "$ref" | grep -Eq '^[A-Za-z0-9_.-]+/[A-Za-z0-9_./-]+@[0-9a-f]{40} # v[0-9]+(\.[0-9]+)*$'; then
            echo "FAIL: $name:${l%%:*}: $ref: not pinned to a commit SHA with its version in a comment"
            bad=$((bad + 1))
        fi
    done <<EOF
$lines
EOF
done

test "$n" -ge 20 || fail "only $n uses: lines found"
test "$bad" -eq 0 || fail "$bad action(s) not pinned"
echo "ok   all $n actions in the workflows are pinned to a commit SHA"
echo "all checks passed"
