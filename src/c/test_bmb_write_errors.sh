#!/bin/sh
# A run whose results cannot be written must fail, not pass with a short or
# missing file (doc/dev/benchmarks.md, "Write failures"): a -o file that
# cannot be opened, a -o file on a full disk, in JSON and in CSV, and
# stdout on a full disk. Each exits non-zero and says what was lost.
# /dev/full, a device on which every write fails, stands for the full
# disk; where there is none, those checks are left out with a note.
set -e

T=write-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

rm -rf "$T"
mkdir "$T"

# fails <message> <stdout> <options...>: runs ddot with stdout to <stdout>,
# which must exit non-zero and say <message>.
fails() {
    want=$1
    out=$2
    shift 2
    if level1/bmb_ddot -C -x 0 -i 1 -v 8 "$@" >"$out" 2>"$T/err"; then
        fail "ddot $* >$out succeeded"
    fi
    grep -q -- "$want" "$T/err" || fail "ddot $* >$out did not say \"$want\": $(cat "$T/err")"
}

fails "Unable to open $T/missing/r.json for writing" /dev/null -o "$T/missing/r.json"
test ! -e "$T/missing" || fail "a directory was created for the -o file"
echo "ok   a -o file that cannot be opened fails the run"

if [ -w /dev/full ]; then
    fails 'Failed to write /dev/full in full; the file is incomplete' /dev/null -o /dev/full -f json
    fails 'Failed to write /dev/full in full; the file is incomplete' /dev/null -o /dev/full -f csv
    fails 'Failed to write the results to stdout' /dev/full
    echo "ok   a full disk, under -o (JSON, CSV) or stdout, fails the run"
else
    echo "note no /dev/full here: the full-disk checks are left out"
fi

rm -rf "$T"
echo "all checks passed"
