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
