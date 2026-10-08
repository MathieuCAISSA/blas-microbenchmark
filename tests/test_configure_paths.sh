#!/bin/sh
# --with-blas-incpath and --with-blas-libpath are the most specific way to
# name a BLAS, so they come first in the search: ahead of a module's
# variables (BLIS_LIBDIR, ...) and of the directories configure finds by
# itself in a distribution's install (/usr/lib/*/blis-openmp, ...). They
# used to be applied before both and come last, and a library named on
# the command line lost to the system's (#58).
#
# Runs configure in a directory of its own, per backend, with a module
# variable and the option both pointing at empty directories, and reads
# the order of the -I and -L flags from config.log. configure then stops,
# finding no library there; the flags are logged by then.
set -e

S=${srcdir:-.}
T=configure-paths.d

fail() {
    echo "FAIL: $*"
    exit 1
}

test -x "$S/configure" || fail "no configure in $S"
src=$(cd "$S" && pwd)
rm -rf "$T"
mkdir -p "$T/mod/include" "$T/mod/lib" "$T/cli/include" "$T/cli/lib"
top=$(cd "$T" && pwd)

# position <flags> <directory>: where the first flag naming it starts, or
# nothing.
position() {
    printf '%s\n' "$1" | awk -v d="$2" '{ i = index($0, d); if (i) print i }'
}

for backend in openblas blis netlib; do
    var=$(printf '%s' "$backend" | tr 'a-z' 'A-Z')
    mkdir -p "$T/$backend"
    (
        cd "$T/$backend"
        env "${var}_INCDIR=$top/mod/include" "${var}_LIBDIR=$top/mod/lib" \
            "$src/configure" --with-blas-backend="$backend" \
            --with-blas-incpath="$top/cli/include" --with-blas-libpath="$top/cli/lib" \
            >configure.out 2>&1 || true
    )
    log=$T/$backend/config.log
    test -s "$log" || fail "$backend: configure wrote no config.log"
    for v in CPPFLAGS LDFLAGS; do
        flags=$(sed -n "s/^$v='\(.*\)'\$/\1/p" "$log" | tail -1)
        case $v in CPPFLAGS) sub=include ;; *) sub=lib ;; esac
        cli=$(position "$flags" "$top/cli/$sub")
        mod=$(position "$flags" "$top/mod/$sub")
        test -n "$cli" && test -n "$mod" || fail "$backend: $v does not name both directories: $flags"
        test "$cli" -lt "$mod" || fail "$backend: $v puts the module's directory before --with-blas-*path's: $flags"
        # Where this machine has the distribution's library, the probe's
        # directory is there too, and must come after.
        for d in /usr/include/*/blis-* /usr/lib/*/blis-* /usr/lib/*/blas; do
            [ -d "$d" ] || continue
            p=$(position "$flags" "$d")
            [ -z "$p" ] || test "$cli" -lt "$p" \
                || fail "$backend: $v puts $d, found by configure, before --with-blas-*path's: $flags"
        done
    done
done
echo "ok   --with-blas-incpath and --with-blas-libpath come first, for openblas, blis and netlib"

rm -rf "$T"
echo "all checks passed"
