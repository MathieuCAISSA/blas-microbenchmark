#!/bin/sh
# bmb_report -- one HTML page of charts from blas-microbenchmark results.
#
#     bmb_report results/*.json > report.html
#     bmb_report results/ > report.html
#     firefox report.html
#
# The page is self-contained: data, styles and script all live inside it and
# nothing is fetched, so it opens from file:// on a machine with no network
# (#1). This file is the source; `make` builds the installed bmb_report from
# it by inserting src/report/report.html at the two markers below.

BMB_VERSION='@PACKAGE_VERSION@'
BMB_ACCEPT_MAJOR='@ACCEPT_MAJOR@'

usage() {
    cat <<'EOF'
Usage: bmb_report [OPTION]... RESULT.json|DIR... > report.html

Turns the JSON results written by the blas-microbenchmark benchmarks
(bmb_<routine> -o result.json) into one self-contained HTML page of charts.
A directory stands for every *.json under it, at any depth.
The page needs no network: open it with any browser.

      --peak-gflops <x>  one core's peak, in GFLOP/s, in double precision
                         (twice that in single): level 3 as a fraction of
                         it, levels 1 and 2 on a roofline; overrides the
                         peak the results carry
      --peak-gbs <y>     the memory bandwidth, in GB/s: the roofline's roof
  -h, --help             show this help
  -V, --version          show the version

Full documentation: man bmb_report
EOF
}

say() {
    printf 'bmb_report: %s\n' "$*" >&2
}

nl='
'

# Returns 0 when $1 is a result this report can read. Otherwise says why on
# stderr and returns 1. Everything is checked before anything is written:
# one refused file means no page at all, because a page silently missing a
# backend would be read as complete (#1, decision 5).
#
# This reads the files line by line rather than parsing JSON, which works
# because the benchmarks write them: one field per line, fixed indentation.
check_file() {
    f=$1

    case $f in
    *"$nl"*)
        say "a file name contains a newline; rename it"
        return 1
        ;;
    esac
    if [ -d "$f" ] || [ ! -r "$f" ]; then
        say "$f: cannot be read"
        return 1
    fi

    v=$(sed -n 's/^  "version": "\([^"]*\)",$/\1/p' "$f" | sed -n 1p)
    if [ -z "$v" ]; then
        if grep -q '^  "results": \[' "$f" && grep -q '^  "routine": "' "$f"; then
            say "$f: produced before 0.6.0, when time_s was a mean rather than the" \
                "fastest sample and no backend was recorded, so it cannot be compared" \
                "with current results. Re-run the benchmark to include it."
        else
            say "$f: not a blas-microbenchmark result"
        fi
        return 1
    fi

    major=${v%%.*}
    case $major in
    '' | *[!0-9]*)
        say "$f: not a blas-microbenchmark result"
        return 1
        ;;
    esac
    if [ "$major" -lt "$BMB_ACCEPT_MAJOR" ]; then
        say "$f: produced by blas-microbenchmark $v, which records neither the" \
            "machine nor the dimension labels this report needs. It reads" \
            "$BMB_ACCEPT_MAJOR.x results; re-run the benchmark to include it."
        return 1
    fi
    if [ "$major" -gt "$BMB_ACCEPT_MAJOR" ]; then
        say "$f: produced by blas-microbenchmark $v; this bmb_report ($BMB_VERSION)" \
            "reads $BMB_ACCEPT_MAJOR.x results. Update blas-microbenchmark."
        return 1
    fi

    # The benchmarks only write the file at the end of a run, but a copy can
    # still be cut short.
    if [ "$(sed -n '$p' "$f")" != "}" ]; then
        say "$f: incomplete (it does not end with '}'); was it copied in full?"
        return 1
    fi

    return 0
}

# JSON-escapes $1, and turns "</" into "<\/" -- the same character as far as
# JSON is concerned, but it stops a "</script>" inside a string from closing
# the element it is embedded in.
json_string() {
    printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's#</#<\\/#g'
}

# peak OPTION VALUE: VALUE as a JSON number if it is a peak, a plain
# decimal above 0 and up to 1000000, as the benchmarks take it (#249);
# else says so and exits.
peak() {
    case $2 in
    '' | *[!0-9.]* | .* | *. | *.*.*)
        say "$1 takes a number above 0 and up to 1000000, e.g. 32 (got '$2')"
        exit 2
        ;;
    esac
    if ! awk -v v="$2" 'BEGIN { exit !(v + 0 > 0 && v + 0 <= 1000000) }'; then
        say "$1 takes a number above 0 and up to 1000000, e.g. 32 (got '$2')"
        exit 2
    fi
    # As a JSON number: 007 is not one, 7 is.
    awk -v v="$2" 'BEGIN { printf "%.15g", v + 0 }'
}

peak_gflops=
peak_gbs=
while [ $# -gt 0 ]; do
    case $1 in
    -h | --help)
        usage
        exit 0
        ;;
    -V | --version)
        printf 'bmb_report %s\n' "$BMB_VERSION"
        exit 0
        ;;
    --peak-gflops | --peak-gbs)
        [ $# -ge 2 ] || { say "$1 needs a value"; exit 2; }
        v=$(peak "$1" "$2") || exit 2
        if [ "$1" = --peak-gflops ]; then peak_gflops=$v; else peak_gbs=$v; fi
        shift 2
        ;;
    --peak-gflops=* | --peak-gbs=*)
        v=$(peak "${1%%=*}" "${1#*=}") || exit 2
        case $1 in --peak-gflops=*) peak_gflops=$v ;; *) peak_gbs=$v ;; esac
        shift
        ;;
    --)
        shift
        break
        ;;
    -*)
        say "unknown option: $1"
        usage >&2
        exit 2
        ;;
    *)
        break
        ;;
    esac
done

if [ $# -eq 0 ]; then
    usage >&2
    exit 2
fi

# Some kilobytes of HTML scrolling past in a terminal help nobody.
if [ -t 1 ]; then
    say "the report is HTML; redirect it to a file: bmb_report $* > report.html"
    exit 2
fi

# A directory stands for every result under it, at any depth, in name
# order, so that the same directory always gives the same page (#253): a
# directory a library is what bmb_run writes. The files taken are kept a
# line each, which a name with a newline would break: refused first.
status=0
files=
for a in "$@"; do
    if [ -d "$a" ]; then
        if [ -n "$(find "$a" -name "*$nl*")" ]; then
            say "$a: a file name under it contains a newline; rename it"
            status=1
            continue
        fi
        if ! found=$(find "$a" -name '*.json' ! -type d); then
            say "$a: cannot be read in full"
            status=1
            continue
        fi
        if [ -z "$found" ]; then
            say "$a: no result (*.json) under it"
            status=1
            continue
        fi
        found=$(printf '%s\n' "$found" | LC_ALL=C sort)
        while IFS= read -r f; do
            check_file "$f" || status=1
        done <<EOF
$found
EOF
        files=$files$found$nl
    else
        check_file "$a" || status=1
        files=$files$a$nl
    fi
done
if [ $status -ne 0 ]; then
    say "no report written"
    exit 1
fi
set -f
IFS=$nl
# shellcheck disable=SC2086 # a file a line, every name checked above
set -- $files
unset IFS
set +f

cat <<'BMB_REPORT_HEAD'
@BMB_REPORT_HEAD@
BMB_REPORT_HEAD

# The peaks are digits and a point, checked above: safe as JSON numbers.
printf '<script type="application/json" id="bmb-meta">{"generator": "bmb_report %s", "date": "%s"%s%s}</script>\n' \
    "$(json_string "$BMB_VERSION")" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    "${peak_gflops:+, \"peak_gflops\": $peak_gflops}" "${peak_gbs:+, \"peak_gbs\": $peak_gbs}"

for f in "$@"; do
    printf '<script type="application/json" class="bmb-result">{"file": "%s", "result":\n' \
        "$(json_string "$f")"
    sed 's#</#<\\/#g' "$f"
    printf '}</script>\n'
done

cat <<'BMB_REPORT_TAIL'
@BMB_REPORT_TAIL@
BMB_REPORT_TAIL
