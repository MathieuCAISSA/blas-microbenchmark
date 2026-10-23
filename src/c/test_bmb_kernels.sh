#!/bin/sh
# The kernels the library's dispatch chose at run time (#162), for the
# backend built (read from config.h): OpenBLAS's core, BLIS's and
# AOCL-BLAS's configuration, MKL's code branch, in -V, the text and the
# JSON, the same in all three; what the library is told to choose, when
# it can be told (OPENBLAS_CORETYPE on a DYNAMIC_ARCH OpenBLAS,
# MKL_ENABLE_INSTRUCTIONS on an Intel CPU), is what is recorded; nothing
# for a library that does not say (Netlib, NVPL, ArmPL, FlexiBLAS).
set -e

T=kernels-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

config=${top_builddir:?top_builddir is not set}/config.h
has() {
    grep -q "^#define $1 1\$" "$config"
}

rm -rf "$T"
mkdir "$T"
B=level1/double/bmb_ddot

# kernels: what -V, the text and the JSON say, the three the same.
kernels() {
    "$B" -V >"$T/v.txt" || fail "$B -V failed"
    "$B" -x 0 -i 1 -w 0 -v 8 -C -o "$T/k.json" >"$T/k.txt" || fail "$B failed"
    v=$(sed -n 's/^Kernels: //p' "$T/v.txt")
    t=$(sed -n 's/^# kernels: //p' "$T/k.txt")
    j=$(sed -n 's/^  "kernels": "\(.*\)",$/\1/p' "$T/k.json")
    test "$v" = "$t" && test "$t" = "$j" || fail "-V says '$v', the text '$t', the JSON '$j'"
    echo "$t"
}

if has HAVE_OPENBLAS_GET_CORENAME || has HAVE_BLI_ARCH_QUERY_ID || has HAVE_MKL_CBWR_GET_AUTO_BRANCH; then
    k=$(kernels)
    test -n "$k" || fail "the library can say which kernels it chose, and nothing was recorded"
    # MKL's branch by its name, AUTO on a CPU MKL names none for (#187).
    if has HAVE_MKL_CBWR_GET_AUTO_BRANCH; then
        case $k in "branch "*) fail "MKL's branch recorded as a number, $k: one constant is not named" ;; esac
    fi
    echo "ok   the kernels the library chose, $k, in -V, the text and the JSON"
else
    k=$(kernels)
    test -z "$k" || fail "kernels '$k' recorded for a library that cannot say"
    echo "ok   no kernels recorded for a library that does not say"
fi

# Told to choose another, it says that one: the run time's choice, not the
# build's.
case $(uname -m) in
    x86_64) core=Prescott ;;
    *) core= ;;
esac
if has HAVE_OPENBLAS_GET_CORENAME && grep -q DYNAMIC_ARCH "$T/v.txt" && [ -n "$core" ]; then
    k=$(OPENBLAS_CORETYPE=$core kernels)
    test "$k" = "$core" || fail "OPENBLAS_CORETYPE=$core, and the kernels recorded are '$k'"
    echo "ok   OPENBLAS_CORETYPE=$core: $core recorded"
fi
# MKL dispatches on what the CPU is as well as what it can do: on an AMD
# EPYC it takes a branch of its own, which MKL_ENABLE_INSTRUCTIONS does
# not move.
if has HAVE_MKL_CBWR_GET_AUTO_BRANCH && grep -q '^vendor_id.*GenuineIntel' /proc/cpuinfo 2>/dev/null; then
    k=$(MKL_ENABLE_INSTRUCTIONS=SSE4_2 kernels)
    test "$k" = SSE4_2 || fail "MKL_ENABLE_INSTRUCTIONS=SSE4_2, and the kernels recorded are '$k'"
    echo "ok   MKL_ENABLE_INSTRUCTIONS=SSE4_2: SSE4_2 recorded"
fi

rm -rf "$T"
echo "all checks passed"
