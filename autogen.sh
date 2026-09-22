#!/bin/sh
# Generates the configure script. Only needed when building from a git
# checkout (or GitHub's auto-generated "Source code" archive) -- release
# tarballs ship configure pre-generated, so this step can be skipped there.
set -e

if [ -f configure ]; then
    echo "configure already exists; regenerating it."
fi

autoreconf -fi

echo
echo "Done. Next:"
echo "    ./configure && make && make check && sudo make install"
