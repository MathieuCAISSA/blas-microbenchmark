#!/bin/sh
# Checks that spack-repo-index.yaml, where Spack looks when the repository
# is added by its git URL, leads to a Spack repository: each of its paths
# holds a repo.yaml whose namespace is the directory's name, and a
# packages/ directory. The spack CI job adds the repository by its path,
# so nothing else reads this file before a user does (#283).
#
# The index is not in the tarball, so this SKIPs there.
set -e

S=${srcdir:-.}

fail() {
    echo "FAIL: $*"
    exit 1
}

index=$S/spack-repo-index.yaml
test -f "$index" || { echo "SKIP: no spack-repo-index.yaml in $S"; exit 77; }

# The entries of paths:, one "  - <path>" a line.
sed -n '/^  paths:$/,/^[^ ]/s/^  - //p' "$index" >spack-paths.tmp
test -s spack-paths.tmp || fail "spack-repo-index.yaml lists no path"
n=0
while read -r p; do
    test -f "$S/$p/repo.yaml" || fail "$p, in spack-repo-index.yaml, holds no repo.yaml"
    ns=$(sed -n "s/^  namespace: '\{0,1\}\([^']*\)'\{0,1\}$/\1/p" "$S/$p/repo.yaml")
    test "$ns" = "$(basename "$p")" || fail "$p/repo.yaml's namespace is '$ns', not its directory's name"
    test -d "$S/$p/packages" || fail "$p holds no packages/"
    n=$((n + 1))
done <spack-paths.tmp
rm -f spack-paths.tmp
echo "ok   the $n path(s) of spack-repo-index.yaml each hold a Spack repository"
