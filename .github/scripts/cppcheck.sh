#!/bin/sh
# Static analysis of the C code with cppcheck: what -Wall -Wextra does not
# see, on paths no test takes. Run from the top of the source tree, after
# configure, with the build directory whose config.h to use:
#
#   .github/scripts/cppcheck.sh build
#
# Any finding fails. A finding that is wrong is suppressed where it is, with
# a comment saying why (/* cppcheck-suppress <id> */ on the line above);
# the two below are suppressed everywhere, for the reasons given.
set -e

build=${1:?usage: $0 BUILD_DIR}
test -f "$build/config.h" || { echo "no $build/config.h: run configure first" >&2; exit 1; }

# variableScope: moving each variable into its innermost block is a style
# this code does not follow (most are declared at the top of their
# function); it is not a defect.
# missingIncludeSystem: cppcheck is not given the system headers; it knows
# the C and POSIX functions from its own library files instead.
run() {
    cppcheck \
        --enable=warning,style,performance,portability \
        --std=c11 \
        --check-level=exhaustive \
        --inline-suppr \
        --error-exitcode=1 \
        --quiet \
        --suppress=variableScope \
        --suppress=missingIncludeSystem \
        -DHAVE_CONFIG_H -I "$build" -I src/c/common \
        --template='{file}:{line}: {severity}: {message} [{id}]' \
        "$@"
}

# A family's source, src/c/level<N>/bmb_<family>.c, is compiled once for
# each precision src/c/routines.txt gives it (#166), and cppcheck, given
# a -D, reads a file in that configuration alone: with none, bmb_prec.h
# stops at its #error. So the family sources are left out of the run over
# the tree, and read in one run per precision, with that precision's.
# family_sources [P]: the family sources, of precision P's routines only.
family_sources() {
    awk -v p="$1" '!/^#/ && NF == 5 && (p == "" || $3 == p) {
        print "src/c/level" $2 "/bmb_" $5 ".c"
    }' src/c/routines.txt | sort -u
}

status=0
ignore=
# Split on purpose: one path per line, none with a space.
for f in $(family_sources); do
    ignore="$ignore -i $f"
done
# shellcheck disable=SC2086 # one word per option and file, none with a space
run $ignore src/c || status=1
for p in s d c z; do
    files=$(family_sources "$p")
    [ -n "$files" ] || continue
    # shellcheck disable=SC2086 # one word per file
    run "-DBMB_PREC_$(echo "$p" | tr sdcz SDCZ)" $files || status=1
done
exit "$status"
