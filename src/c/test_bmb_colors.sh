#!/bin/sh
# Warnings and errors are coloured on a terminal and nowhere else: in red
# and yellow when stderr is one, plain when NO_COLOR is set and not empty,
# plain when stderr goes to a file or a pipe, where an escape sequence
# would end up in what the user is collecting. The terminal is util-linux's
# script(1), which runs the benchmark on a pseudo-terminal; SKIPs without
# it.
set -e

fail() {
    echo "FAIL: $*"
    exit 1
}

command -v script >/dev/null 2>&1 || { echo "SKIP: no script(1) to give the benchmark a terminal"; exit 77; }
script -qec true /dev/null >/dev/null 2>&1 || { echo "SKIP: script(1) cannot open a terminal here"; exit 77; }

esc=$(printf '\033')
prog=./level3/double/bmb_dgemm

# An error (-m 0 is refused) and a warning (-L on a level 1 routine).
NO_COLOR= script -qec "$prog -m 0" /dev/null >colors.tty 2>&1 || true
grep -q "${esc}\[31mInvalid value for --matrix-dim1" colors.tty \
    || fail "an error on a terminal is not in red: $(od -c colors.tty | head -3)"
grep -q "${esc}\[0m" colors.tty || fail "the colour is not reset after the error"
echo "ok   an error on a terminal is red"

NO_COLOR= script -qec "./level1/double/bmb_ddot -x 0 -i 1 -v 8 -L col" /dev/null >colors.tty 2>&1 || true
grep -q "${esc}\[33m" colors.tty || fail "a warning on a terminal is not in yellow: $(od -c colors.tty | head -3)"
echo "ok   a warning on a terminal is yellow"

NO_COLOR=1 script -qec "$prog -m 0" /dev/null >colors.tty 2>&1 || true
grep -q "Invalid value for --matrix-dim1" colors.tty || fail "no error with NO_COLOR=1: $(cat colors.tty)"
if grep -q "$esc" colors.tty; then
    fail "NO_COLOR=1 still coloured the error"
fi
echo "ok   NO_COLOR=1 turns the colours off"

NO_COLOR= $prog -m 0 >/dev/null 2>colors.err || true
grep -q "Invalid value for --matrix-dim1" colors.err || fail "no error on a redirected stderr: $(cat colors.err)"
if grep -q "$esc" colors.err; then
    fail "an error written to a file was coloured"
fi
echo "ok   no colour in a file"

rm -f colors.tty colors.err
