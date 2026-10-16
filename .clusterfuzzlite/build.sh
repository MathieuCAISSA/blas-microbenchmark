#!/bin/bash -eu
# Builds the fuzz targets into $OUT, as ClusterFuzzLite (and OSS-Fuzz)
# expect: configure with the sanitizer flags ClusterFuzzLite sets in CC and
# CFLAGS, the common layer as usual, then each target linked against
# libFuzzer, with its seed corpus and its dictionary beside it.

./autogen.sh
./configure
make -C src/c/common libbmbcommon.a

libs=$(sed -n 's/^LIBS = //p' Makefile)

# shellcheck disable=SC2086 # $CFLAGS, $LIB_FUZZING_ENGINE and $libs are lists of flags
$CC $CFLAGS -DBMB_LIBFUZZER -Isrc/c/common \
    -c src/c/common/fuzz_bmb_options.c -o fuzz_bmb_options.o
# The libraries go in $OUT/lib (below). An RPATH rather than a RUNPATH
# (--disable-new-dtags): a RUNPATH only serves the target's own
# dependencies, and libopenblas would not find its libgfortran there.
# shellcheck disable=SC2086,SC2016 # $ORIGIN is for the dynamic linker, not the shell
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE fuzz_bmb_options.o src/c/common/libbmbcommon.a \
    $libs -Wl,--disable-new-dtags,-rpath,'$ORIGIN/lib' -o "$OUT/fuzz_bmb_options"

# The fuzzers run in another image, which has no BLAS: the shared
# libraries the target needs beyond the C and C++ runtimes go with it.
mkdir -p "$OUT/lib"
ldd "$OUT/fuzz_bmb_options" | awk '$3 ~ /^\// { print $3 }' \
    | grep -E 'openblas|gfortran|quadmath|blas|omp' \
    | while read -r lib; do cp -L "$lib" "$OUT/lib/"; done

cp src/c/common/fuzz/bmb_options.dict "$OUT/fuzz_bmb_options.dict"
zip -jq "$OUT/fuzz_bmb_options_seed_corpus.zip" src/c/common/fuzz/bmb_options/*
