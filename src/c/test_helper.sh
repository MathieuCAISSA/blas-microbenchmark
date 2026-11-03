# shellcheck shell=sh
# Shared assertions for the per-routine smoke tests, sourced as:
#     . "${srcdir:-.}/../test_helper.sh"
#
# Keep the sizes in every caller small. These run on every `make check`,
# and a benchmark asked for a large size allocates it for real: a vector of
# 1.5e9 doubles is 12 GB, and two of them will take the machine down long
# before the test gets around to failing.

bmb_fail() {
    echo "FAIL: $*"
    exit 1
}

# bmb_missing <routine>: true when the routine is an extension of the
# BLAS (#171, "ext" in routines.txt) that this build's library does not
# have (no HAVE_CBLAS_<FAMILY> in config.h), so that its program skips.
# For the tests that run every benchmark: routines.txt is bmb_table, by
# default in their srcdir (src/c's), and top_builddir in their
# environment.
bmb_missing() {
    bmb_family=`awk -v r="$1" '!/^#/ && $1 == r && $6 == "ext" { print toupper($5) }' "${bmb_table:-${srcdir:-.}/routines.txt}"`
    [ -n "$bmb_family" ] || return 1
    ! grep -q "^#define HAVE_CBLAS_$bmb_family 1\$" "${top_builddir:?top_builddir is not set}/config.h"
}

# bmb_known_wrong <program> <routine> [options...]: when the library is
# known to get this call wrong, a pattern of what --verify says of it,
# which a test then requires in place of a pass; nothing otherwise. For a
# library whose version cannot be read (an OpenBLAS FlexiBLAS loads),
# "maybe " before it: a pass, or that. OpenBLAS's gemmt, before 0.3.29,
# filled the other triangle in row-major (OpenMathLib/OpenBLAS#4905);
# before 0.3.30, it conjugated the operand op() takes the conjugate
# transpose of in place, B column-major, A row-major, where A and B
# change places (#5143). Its cblas_?gemm_batch_strided, from 0.3.31 to
# 0.3.34, refused every column-major call and left C as it was (#5979);
# from 0.3.31, its complex one does not scale the strides to (re, im)
# pairs, wrong from two matrices on (#6214, reported, not fixed: when it
# is, the test on the fixed version fails, and the bound goes here). The
# others are fixed upstream, and a test that still meets them on a fixed
# version fails.
bmb_known_wrong() {
    kw_prog=$1
    kw_r=$2
    shift 2
    case $kw_r in ?gemmt | ?gemm) ;; *) return 0 ;; esac
    kw_backend=`"$kw_prog" --version 2>/dev/null | sed -n 's/^BLAS backend: //p'`
    case $kw_backend in
        "openblas (OpenBLAS "*)
            kw_maybe=
            # A development build, "0.3.34.dev", has the fixes of the
            # release it leads to: taken as 0.3.35.
            kw_v=`printf '%s\n' "$kw_backend" | sed -n -e 's/^openblas (OpenBLAS \([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.dev.*/\1 \2 \3 1/p' \
                -e 's/^openblas (OpenBLAS \([0-9]*\)\.\([0-9]*\)\.\([0-9]*\).*/\1 \2 \3 0/p' | sed -n 1p`
            # shellcheck disable=SC2086 # three numbers
            set -- "$@" -- $kw_v
            ;;
        flexiblas/openblas*) kw_maybe="maybe " ; set -- "$@" -- 0 3 29 0 ;;
        *) return 0 ;;
    esac
    kw_trans=NN kw_layout=row kw_mode=loop
    while [ "$1" != -- ]; do
        case $1 in
            --trans=*) kw_trans=${1#--trans=} ;;
            --trans) kw_trans=$2; shift ;;
            --layout=*) kw_layout=${1#--layout=} ;;
            --layout | -L) kw_layout=$2; shift ;;
            --batch-mode=*) kw_mode=${1#--batch-mode=} ;;
            --batch-mode) kw_mode=$2; shift ;;
        esac
        shift
    done
    shift
    kw_at=$(( ${1:-0} * 10000 + ${2:-0} * 100 + ${3:-0} + ${4:-0} ))
    case $kw_r in
        [cz]gemm)
            if [ "$kw_mode" = strided ] && [ "$kw_at" -ge 331 ]; then
                echo "${kw_maybe}Wrong result: "
            fi
            return 0
            ;;
        ?gemm)
            if [ "$kw_mode$kw_layout" = stridedcol ] && [ "$kw_at" -ge 331 ] && [ "$kw_at" -lt 335 ]; then
                echo "${kw_maybe}Wrong result: "
            fi
            return 0
            ;;
    esac
    if [ "$kw_layout" = row ] && [ "$kw_at" -lt 329 ]; then
        echo "${kw_maybe}Wrong result: "
    elif [ "$kw_at" -lt 330 ]; then
        case $kw_layout$kw_trans in
            col?C) echo "${kw_maybe}the call changed B, which it must only read" ;;
            rowC?) echo "${kw_maybe}the call changed A, which it must only read" ;;
        esac
    fi
}

# bmb_batch_crash <program> <mode>: when the library would crash on a
# batch of small matrices in this --batch-mode (#330), which the program
# refuses (exit 77), the upstream issue its message names; nothing
# otherwise. OpenBLAS 0.3.28 to 0.3.34 built with DYNAMIC_ARCH, where it
# has its small-matrix path: the group call from 0.3.28
# (OpenMathLib/OpenBLAS#5980), the strided one from 0.3.31 (#6086).
bmb_batch_crash() {
    case $2 in group | strided) ;; *) return 0 ;; esac
    case `uname -m` in x86_64 | aarch64 | arm64 | ppc64* | loongarch64) ;; *) return 0 ;; esac
    bc_backend=`"$1" --version 2>/dev/null | sed -n 's/^BLAS backend: //p'`
    case $bc_backend in
        "openblas (OpenBLAS "*" DYNAMIC_ARCH "* | "openblas (OpenBLAS "*" DYNAMIC_ARCH)") ;;
        *) return 0 ;;
    esac
    bc_at=`printf '%s\n' "$bc_backend" | sed -n -e 's/^openblas (OpenBLAS \([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\.dev.*/\1 \2 \3 1/p' \
        -e 's/^openblas (OpenBLAS \([0-9]*\)\.\([0-9]*\)\.\([0-9]*\).*/\1 \2 \3 0/p' | sed -n 1p |
        awk '{ print $1 * 10000 + $2 * 100 + $3 + $4 }'`
    [ -n "$bc_at" ] && [ "$bc_at" -le 334 ] || return 0
    if [ "$2" = strided ] && [ "$bc_at" -ge 331 ]; then
        echo "OpenMathLib/OpenBLAS#6086"
    elif [ "$2" = group ] && [ "$bc_at" -ge 328 ]; then
        echo "OpenMathLib/OpenBLAS#5980"
    fi
}

# bmb_known_wrong_run <program> <routine> [options...]: where the library
# is known to get the call wrong (bmb_known_wrong), runs it and requires
# --verify to say so, exit 2, then ends the test there (exit 0): its rows
# would be those of a library computing the wrong thing.
bmb_known_wrong_run() {
    kw_want=`bmb_known_wrong "$@"`
    [ -n "$kw_want" ] || return 0
    kw_prog=$1
    kw_routine=$2
    shift 2
    kw_status=0
    kw_err=`"$kw_prog" -x 0 -w 0 -i 1 "$@" 2>&1 >/dev/null` || kw_status=$?
    case $kw_want in
        "maybe "*)
            [ "$kw_status" -eq 0 ] && return 0
            kw_want=${kw_want#maybe }
            ;;
    esac
    [ "$kw_status" -eq 2 ] || bmb_fail "$kw_routine: a library known to get it wrong, and exit $kw_status, not 2: $kw_err"
    case $kw_err in
        *"$kw_want"*) ;;
        *) bmb_fail "$kw_routine: not '$kw_want': $kw_err" ;;
    esac
    echo "ok   $kw_routine: this library's known bug, caught: $kw_err"
    exit 0
}

# bmb_skip_missing <program> <routine> [options...]
#
# Exits 77, the test skipped, when the routine is an extension of the
# BLAS (#171) that this build's library does not have: the program then
# exits 77 itself, and must say which call is missing. The options are a
# tiny size, run once where the routine is there.
bmb_skip_missing() {
    bmb_prog=$1
    bmb_routine=$2
    shift 2

    bmb_status=0
    bmb_err=`"$bmb_prog" -x 0 -w 0 -i 1 "$@" 2>&1 >/dev/null` || bmb_status=$?
    if [ "$bmb_status" -eq 77 ]; then
        echo "$bmb_err" | grep -q "$bmb_routine: this build's .* has no cblas_" \
            || bmb_fail "$bmb_routine: skipped without saying which call is missing: $bmb_err"
        echo "SKIP: $bmb_err"
        exit 77
    fi
}

# bmb_check_run <program> <routine> <expected data rows> [options...]
#
# Runs the benchmark and checks the shape of what it printed: the routine
# header, a timing column, the expected number of data rows, and that every
# field on those rows is a positive number. Exit status alone would accept
# a benchmark that printed nothing at all, or one whose timings had
# collapsed to zero.
bmb_check_run() {
    bmb_prog=$1
    bmb_routine=$2
    bmb_rows=$3
    shift 3

    bmb_out=`"$bmb_prog" "$@"` || bmb_fail "$bmb_routine: exited $?"

    # The provenance block and the routine name are comment lines; the
    # table starts at the first line that is not one.
    echo "$bmb_out" | grep -q "^# routine: $bmb_routine\$" \
        || bmb_fail "$bmb_routine: no '# routine: $bmb_routine' line"

    echo "$bmb_out" | grep -q "^# blas-microbenchmark [0-9]" \
        || bmb_fail "$bmb_routine: no version line"

    echo "$bmb_out" | grep -q "^# backend: " \
        || bmb_fail "$bmb_routine: no backend line"

    bmb_table=`echo "$bmb_out" | grep -v "^#"`

    echo "$bmb_table" | sed -n 1p | grep -q 'time \[s\]' \
        || bmb_fail "$bmb_routine: no 'time [s]' column in the header"

    # The time and the median are the only columns printed with nine
    # decimals without -s, in that order, which is what identifies them
    # here without the helper having to know each routine's column layout.
    # The median of the samples is never faster than the fastest one.
    echo "$bmb_table" | awk -v routine="$bmb_routine" -v want="$bmb_rows" '
        NR == 1 { next }
        {
            rows++
            if (fields == 0) {
                fields = NF
            } else if (NF != fields) {
                printf "FAIL: %s: row %d has %d fields, earlier rows had %d\n",
                       routine, rows, NF, fields
                bad = 1
            }

            times = 0
            fastest = -1
            for (i = 1; i <= NF; i++) {
                if ($i !~ /^[0-9]+(\.[0-9]+)?$/) {
                    printf "FAIL: %s: row %d field %d is \"%s\", not a number\n",
                           routine, rows, i, $i
                    bad = 1
                } else if ($i + 0 < 0) {
                    printf "FAIL: %s: row %d field %d is negative\n", routine, rows, i
                    bad = 1
                }
                if ($i ~ /^[0-9]+\.[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]$/) {
                    times++
                    if ($i + 0 <= 0) {
                        printf "FAIL: %s: row %d reports a time of %s\n", routine, rows, $i
                        bad = 1
                    }
                    if (fastest < 0) {
                        fastest = $i + 0
                    } else if ($i + 0 < fastest) {
                        printf "FAIL: %s: row %d has a median of %s, faster than its time\n",
                               routine, rows, $i
                        bad = 1
                    }
                }
            }
            if (times != 2) {
                printf "FAIL: %s: row %d has %d timing columns, expected 2: the time and the median\n",
                       routine, rows, times
                bad = 1
            }
        }
        END {
            if (rows != want) {
                printf "FAIL: %s: %d data rows, expected %d\n", routine, rows, want
                bad = 1
            }
            if (fields < 3) {
                printf "FAIL: %s: only %d columns\n", routine, fields
                bad = 1
            }
            exit bad ? 1 : 0
        }
    ' || exit 1

    echo "ok   $bmb_routine"
}

# bmb_check_too_large <program> <routine> [options...]
#
# Checks that the benchmark refuses the sizes it is given for what they
# would take in bytes, before allocating anything: a non-zero exit and
# its own message. A malloc of the wrapped or huge size failing instead
# would exit too, saying only that setup failed, which is not the check.
bmb_check_too_large() {
    bmb_prog=$1
    bmb_routine=$2
    shift 2

    if "$bmb_prog" "$@" -x 0 -i 1 -C >/dev/null 2>"$bmb_routine-too-large.err"; then
        bmb_fail "$bmb_routine: $* was accepted"
    fi
    grep -q 'more bytes than size_t can count' "$bmb_routine-too-large.err" \
        || bmb_fail "$bmb_routine: $* was not refused for its size: $(cat "$bmb_routine-too-large.err")"
    rm -f "$bmb_routine-too-large.err"
    echo "ok   $bmb_routine refuses $*"
}
