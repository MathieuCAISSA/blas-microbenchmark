#!/bin/sh
# The Spack package (packaging/spack) built against one BLAS and checked by
# its own tests (spack test run), twice:
#
#     .github/spack-check.sh BLAS SRC
#
# - this commit, from SRC, the unpacked tarball of `make dist`, through a
#   Spack environment that develops it: the recipe as it is, on the code
#   as it is;
# - the latest release, as users get it: Spack fetches the tarball from
#   GitHub and checks its sha256 against the recipe.
#
# BLAS is a provider Spack builds (openblas, blis, netlib-lapack,
# flexiblas). spack must be on PATH, with this repository's package
# repository added. The CI job installs a distribution's BLAS too where it
# can: the package's test_linked_library then fails if the benchmarks
# load that one instead of the one Spack built (#58).
set -e

blas=${1:?usage: $0 BLAS SRC}
src=${2:?usage: $0 BLAS SRC}
src=$(cd "$src" && pwd)
version=$(basename "$src" | sed 's/^blas-microbenchmark-//')
env=${TMPDIR:-/tmp}/spack-check-$blas
jobs=$(nproc 2>/dev/null || echo 2)

echo "::group::this commit ($version) ^$blas"
rm -rf "$env"
spack env create -d "$env"
spack -e "$env" add "blas-microbenchmark@=$version ^[virtuals=blas] $blas"
spack -e "$env" develop --no-clone --path "$src" "blas-microbenchmark@=$version"
spack -e "$env" install -j "$jobs" --fail-fast
# spack test run exits non-zero when a test fails; the details are in
# spack test results.
spack -e "$env" test run --alias "dev-$blas" blas-microbenchmark \
    || { spack test results -l "dev-$blas"; exit 1; }
echo "::endgroup::"

echo "::group::the release ^$blas"
spack install -j "$jobs" --fail-fast "blas-microbenchmark@1.4.0 ^[virtuals=blas] $blas"
spack test run --alias "release-$blas" "blas-microbenchmark@1.4.0 ^[virtuals=blas] $blas" \
    || { spack test results -l "release-$blas"; exit 1; }
echo "::endgroup::"

echo "ok   blas-microbenchmark ^$blas: this commit and 1.4.0 build with Spack and pass spack test"
