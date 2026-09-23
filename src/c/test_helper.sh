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

    echo "$bmb_out" | sed -n 1p | grep -q "^# routine: $bmb_routine\$" \
        || bmb_fail "$bmb_routine: first line is not '# routine: $bmb_routine'"

    echo "$bmb_out" | sed -n 2p | grep -q 'time \[s\]' \
        || bmb_fail "$bmb_routine: no 'time [s]' column in the header"

    # The time is the only column printed with nine decimals, which is what
    # identifies it here without the helper having to know each routine's
    # column layout.
    echo "$bmb_out" | awk -v routine="$bmb_routine" -v want="$bmb_rows" '
        NR <= 2 { next }
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
                }
            }
            if (times != 1) {
                printf "FAIL: %s: row %d has %d timing columns, expected 1\n",
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
