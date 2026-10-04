#!/bin/sh
# Checks that CHANGELOG.md and CITATION.cff agree with the version being
# built: a -dev version has an [Unreleased] section; a release has its own
# dated section, and is the version CITATION.cff cites, on the same date.
# Between releases, CITATION.cff cites the latest released version.
#
# Both files are easy to forget when cutting a release, and a citation of
# the wrong version is worse than none.
set -e

S=${srcdir:-.}
V=${PACKAGE_VERSION:?PACKAGE_VERSION is not set}

fail() {
    echo "FAIL: $*"
    exit 1
}

ok() {
    echo "ok   $*"
}

for f in CHANGELOG.md CITATION.cff; do
    test -s "$S/$f" || fail "no $f"
done

# The latest released version in the changelog, and its date.
latest=$(sed -n 's/^## \[\([0-9][0-9.]*\)\] - \([0-9-]*\)$/\1 \2/p' "$S/CHANGELOG.md" | head -1)
test -n "$latest" || fail "CHANGELOG.md has no released version (## [X.Y.Z] - YYYY-MM-DD)"
latest_version=${latest% *}
latest_date=${latest#* }

case $V in
    *-dev)
        grep -q '^## \[Unreleased\]$' "$S/CHANGELOG.md" || fail "a -dev version, and CHANGELOG.md has no [Unreleased] section"
        ok "$V: CHANGELOG.md has an [Unreleased] section"
        ;;
    *)
        test "$latest_version" = "$V" || fail "this is release $V, and the latest version in CHANGELOG.md is $latest_version"
        if grep -q '^## \[Unreleased\]$' "$S/CHANGELOG.md"; then
            fail "this is release $V, and CHANGELOG.md still has an [Unreleased] section"
        fi
        ok "release $V: CHANGELOG.md has its section, dated $latest_date"
        ;;
esac

cited=$(sed -n 's/^version: *//p' "$S/CITATION.cff")
cited_date=$(sed -n 's/^date-released: *//p' "$S/CITATION.cff")
test "$cited" = "$latest_version" || fail "CITATION.cff cites $cited, and the latest release is $latest_version"
test "$cited_date" = "$latest_date" || fail "CITATION.cff dates $cited $cited_date, and CHANGELOG.md $latest_date"
for key in cff-version message title authors license repository-code; do
    grep -q "^$key:" "$S/CITATION.cff" || fail "CITATION.cff has no $key"
done
ok "CITATION.cff cites $cited, released $cited_date"

echo "all checks passed"
