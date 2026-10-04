#!/bin/sh
# Static analysis of the C code with cppcheck: what -Wall -Wextra does not
# see, on paths no test takes. Run from the top of the source tree, after
# configure, with the build directory whose config.h to use:
#
#   .github/cppcheck.sh build
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
exec cppcheck \
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
    src/c
