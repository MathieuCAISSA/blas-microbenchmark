#!/bin/sh
# bmb_suite -- every benchmark of blas-microbenchmark, or a selection, into
# one directory of results, for bmb_report (#254):
#
#     bmb_suite -o results/ --profile quick -- -t 1,8
#     bmb_report results/ > report.html
#
# This file is the source; `make` builds the installed bmb_suite from it by
# substituting the version and where the benchmarks are installed.

BMB_VERSION='@PACKAGE_VERSION@'
BMB_LIBEXEC='@pkglibexecdir@'

# Nothing here needs a glob, and a list given to --routine is no pattern.
set -f

# Every routine, as routine:level:precision (s, d, c or z): where its
# benchmark is, and what --level and --precision select.
# BEGIN routines (suite): written by src/c/routines.sh
ROUTINES='
    sasum:1:s dasum:1:d scasum:1:c dzasum:1:z saxpy:1:s daxpy:1:d caxpy:1:c
    zaxpy:1:z scopy:1:s dcopy:1:d ccopy:1:c zcopy:1:z sdot:1:s ddot:1:d
    cdotc:1:c zdotc:1:z cdotu:1:c zdotu:1:z snrm2:1:s dnrm2:1:d scnrm2:1:c
    dznrm2:1:z sscal:1:s dscal:1:d cscal:1:c zscal:1:z sswap:1:s dswap:1:d
    cswap:1:c zswap:1:z isamax:1:s idamax:1:d icamax:1:c izamax:1:z srot:1:s
    drot:1:d sgemv:2:s dgemv:2:d cgemv:2:c zgemv:2:z sgbmv:2:s dgbmv:2:d
    sger:2:s dger:2:d cgeru:2:c zgeru:2:z cgerc:2:c zgerc:2:z ssymv:2:s
    dsymv:2:d chemv:2:c zhemv:2:z sspmv:2:s dspmv:2:d ssyr:2:s dsyr:2:d
    cher:2:c zher:2:z ssyr2:2:s dsyr2:2:d cher2:2:c zher2:2:z strmv:2:s
    dtrmv:2:d ctrmv:2:c ztrmv:2:z strsv:2:s dtrsv:2:d ctrsv:2:c ztrsv:2:z
    stpsv:2:s dtpsv:2:d stbsv:2:s dtbsv:2:d sgemm:3:s dgemm:3:d cgemm:3:c
    zgemm:3:z ssymm:3:s dsymm:3:d csymm:3:c zsymm:3:z chemm:3:c zhemm:3:z
    ssyrk:3:s dsyrk:3:d csyrk:3:c zsyrk:3:z cherk:3:c zherk:3:z ssyr2k:3:s
    dsyr2k:3:d csyr2k:3:c zsyr2k:3:z cher2k:3:c zher2k:3:z strmm:3:s dtrmm:3:d
    ctrmm:3:c ztrmm:3:z strsm:3:s dtrsm:3:d ctrsm:3:c ztrsm:3:z
'
# END routines (suite)

usage() {
    cat <<'EOF'
Usage: bmb_suite [OPTION]... -o DIR [-- BENCHMARK OPTION...]

Runs every benchmark of blas-microbenchmark, or a selection, one after
the other, and writes their results into DIR, a JSON file a routine, for
bmb_report DIR. The options after -- go to every benchmark, e.g. -t 1,8.
With --blas, the same for each library, into DIR/NAME.

  -o, --output <dir>       where the results go: created, and refused when
                           it already holds files
      --level <list>       only these levels, e.g. 1,2
      --precision <list>   only these precisions, of s, d, c and z, e.g. d,z
      --routine <list>     only these routines, e.g. dgemm,ddot
      --profile <profile>  the sizes: quick, full or a file (default: each
                           level's default size)
      --blas <name>=<suite>
                           also run the blas-microbenchmark of <suite>, an
                           install prefix or a build tree, into DIR/<name>;
                           once a library (default: the suite bmb_suite
                           is installed with, into DIR)
  -n, --dry-run            print the commands, run nothing
  -h, --help               show this help
  -V, --version            show the version

Full documentation: man bmb_suite
EOF
}

nl='
'

say() {
    printf 'bmb_suite: %s\n' "$*" >&2
}

# refuse MESSAGE: a usage error, said before anything has run.
refuse() {
    say "$*"
    exit 2
}

# The profiles shipped, in the format of a profile file: a line a level,
# or a routine, which wins over its level's. The band routines get a band
# narrower than their matrix, which -M gives them; their default is a
# band as wide as it.
profile_quick() {
    cat <<'EOF'
# quick: a few sizes a level, about a minute on one core of a laptop
level1 -v 1000,100000,10000000
level2 -m 200,2000
level3 -m 200,1000
sgbmv -m 200,2000 -M 32
dgbmv -m 200,2000 -M 32
stbsv -m 200,2000 -M 32
dtbsv -m 200,2000 -M 32
EOF
}

profile_full() {
    cat <<'EOF'
# full: sizes doubling, from the first-level cache to memory at levels 1
# and 2, up to 2048 at level 3; several minutes on the same core
level1 -v 256:16777216
level2 -m 32:4096
level3 -m 32:2048
sgbmv -m 32:4096 -M 32
dgbmv -m 32:4096 -M 32
stbsv -m 32:4096 -M 32
dtbsv -m 32:4096 -M 32
EOF
}

# own_option WORD: true when WORD is an option bmb_suite cannot pass on to
# the benchmarks: -o and -f, which it sets itself, and --peak-gflops, a
# core's peak in the routine's precision, which one value would give
# wrong for half the routines. getopt_long takes a long option by any
# prefix that is not ambiguous, hence --o and --peak-gf.
own_option() {
    case $1 in
    -o* | -f* | --o* | --peak-gf*) return 0 ;;
    esac
    return 1
}

# in_list WORD LIST: true when LIST (words, or empty for all) has WORD.
in_list() {
    [ -z "$2" ] && return 0
    case " $2 " in
    *" $1 "*) return 0 ;;
    esac
    return 1
}

# words OPTION VALUE EXAMPLE ITEM WHAT: VALUE, a list, its commas turned
# into spaces, each item checked to be the extended regular expression
# ITEM, else refused as not WHAT, or as no list if empty.
words() {
    w=$(printf '%s\n' "$2" | tr ',' ' ')
    case $w in
    *[!\ ]*) ;;
    *) refuse "$1 takes a list, e.g. $1 $3" ;;
    esac
    for x in $w; do
        printf '%s\n' "$x" | grep -Eqx "$4" || refuse "$1: '$x' is not $5"
    done
    printf '%s\n' "$w"
}

# quoted WORD...: the words, as a shell would read them back.
quoted() {
    for q; do
        case $q in
        '' | *[!A-Za-z0-9_./:,=+@%-]*)
            printf " '%s'" "$(printf '%s' "$q" | sed "s/'/'\\\\''/g")"
            ;;
        *) printf ' %s' "$q" ;;
        esac
    done
}

out=
specs=
names=
levels=
precisions=
routines=
profile_name=
dry=0
while [ $# -gt 0 ]; do
    case $1 in
    -h | --help)
        usage
        exit 0
        ;;
    -V | --version)
        printf 'bmb_suite %s\n' "$BMB_VERSION"
        exit 0
        ;;
    -n | --dry-run)
        dry=1
        shift
        ;;
    -o | --output | --level | --precision | --routine | --profile | --blas)
        [ $# -ge 2 ] || refuse "$1 needs a value"
        opt=$1 val=$2
        shift 2
        ;;
    --output=* | --level=* | --precision=* | --routine=* | --profile=* | --blas=*)
        opt=${1%%=*} val=${1#*=}
        shift
        ;;
    --)
        shift
        break
        ;;
    *)
        say "unknown option: $1"
        usage >&2
        exit 2
        ;;
    esac
    case ${opt-} in
    -o | --output) out=$val ;;
    --level) levels=$(words "$opt" "$val" 1,2 '[123]' 'a level, 1, 2 or 3') || exit 2 ;;
    --precision) precisions=$(words "$opt" "$val" d,z '[sdcz]' 'a precision, s, d, c or z') || exit 2 ;;
    --routine) routines=$(words "$opt" "$val" dgemm,ddot '[a-z0-9]+' 'a routine') || exit 2 ;;
    --profile) profile_name=$val ;;
    --blas)
        # A name is a directory of the results, and a label: kept plain.
        name=${val%%=*}
        case $val in
        *=?*) ;;
        *) refuse "--blas takes <name>=<suite>, e.g. --blas openblas=/opt/bmb-openblas (got '$val')" ;;
        esac
        printf '%s\n' "$name" | grep -Eqx '[A-Za-z0-9_][A-Za-z0-9._-]*' ||
            refuse "--blas: '$name' is not a name: letters, digits, '.', '_' and '-'"
        case $val in
        *"$nl"*) refuse "--blas: a suite's path contains a newline" ;;
        esac
        in_list "$name" "$names" && [ -n "$names" ] && refuse "--blas: $name is given twice"
        names="$names $name"
        specs="$specs$val$nl"
        ;;
    esac
    unset opt
done

[ -n "$out" ] || refuse "where should the results go? Give a directory with -o, e.g. -o results"
case $out in
?*/) out=${out%/} ;;
esac
for w in "$@"; do
    own_option "$w" && refuse "$w: bmb_suite sets -o and -f itself, and the peak is given to bmb_report"
    # getopt_long takes --lab for --label; --la is --layout's too.
    case $w in
    -l* | --lab*)
        [ -n "$specs" ] && refuse "$w: with --blas, bmb_suite labels each library's results with its name"
        ;;
    esac
done

known=
for e in $ROUTINES; do
    known="$known ${e%%:*}"
done
for r in $routines; do
    in_list "$r" "$known" || refuse "--routine: no routine $r in blas-microbenchmark $BMB_VERSION"
done

# The profile, as lines "selector option...", each checked: a selector is
# level1, level2, level3 or a routine, given once.
profile=
case $profile_name in
'') ;;
quick) profile=$(profile_quick) ;;
full) profile=$(profile_full) ;;
*)
    [ -f "$profile_name" ] && [ -r "$profile_name" ] ||
        refuse "--profile: $profile_name is neither quick, full nor a file that can be read"
    profile=$(cat "$profile_name") || refuse "--profile: $profile_name cannot be read"
    ;;
esac
profile=$(awk -v name="$profile_name" -v known="$known" '
    BEGIN { n = split(known, k, " "); for (i = 1; i <= n; i++) { routine[k[i]] = 1 } }
    { sub(/#.*/, "") }
    NF == 0 { next }
    {
        if ($1 !~ /^level[123]$/ && !($1 in routine)) {
            printf "%s:%d: %s is neither level1, level2, level3 nor a routine\n", name, NR, $1 >"/dev/stderr"
            exit 1
        }
        if ($1 in seen) {
            printf "%s:%d: %s is given twice\n", name, NR, $1 >"/dev/stderr"
            exit 1
        }
        seen[$1] = 1
        $1 = $1
        print
    }' <<EOF
$profile
EOF
) || refuse "--profile: $profile_name is not a profile (a line a level or a routine, then its options)"
for w in $profile; do
    own_option "$w" && refuse "--profile: $profile_name gives $w: bmb_suite sets -o and -f itself, and the peak is given to bmb_report"
done

# profile_options ROUTINE LEVEL: the profile's options for ROUTINE.
profile_options() {
    awk -v r="$1" -v l="level$2" '
        $1 == r { mine = $0; sub(/^[^ ]* ?/, "", mine); found = 1 }
        $1 == l { level = $0; sub(/^[^ ]* ?/, "", level) }
        END { print (found ? mine : level) }' <<EOF
$profile
EOF
}

# resolve SPEC: the suite SPEC, <name>=<path>, gives, in name, suite and
# tree (1 for a build tree, a directory a precision under each level);
# false when <path> holds none. An empty SPEC is bmb_suite's own: the
# build tree it is in, or the suite it was installed with.
resolve() {
    name=${1%%=*}
    p=${1#*=}
    tree=0
    if [ -z "$1" ]; then
        here=$(dirname "$0")
        if [ -f "$here/Makefile" ] && [ -d "$here/../c/level1" ]; then
            suite=$here/../c
            tree=1
        else
            suite=$BMB_LIBEXEC
        fi
    elif [ -d "$p/libexec/blas-microbenchmark/level1" ]; then
        suite=$p/libexec/blas-microbenchmark
    elif [ -d "$p/src/c/level1" ]; then
        suite=$p/src/c
        tree=1
    elif [ -d "$p/level1" ]; then
        suite=$p
        [ -d "$p/level1/double" ] && tree=1
    else
        return 1
    fi
    return 0
}

# bench ROUTINE LEVEL PRECISION: where the suite's benchmark is.
bench() {
    if [ "$tree" = 1 ]; then
        case $3 in
        s) d=single ;;
        d) d=double ;;
        c) d=complex ;;
        z) d=complex16 ;;
        esac
        printf '%s/level%s/%s/bmb_%s\n' "$suite" "$2" "$d" "$1"
    else
        printf '%s/level%s/bmb_%s\n' "$suite" "$2" "$1"
    fi
}

# choose: the routines selected that the suite has, in selected and
# count, and those it lacks, in missing. A routine newer than an install
# is left out, unless asked for; a suite with none selected is refused.
choose() {
    selected=
    count=0
    missing=
    for e in $ROUTINES; do
        r=${e%%:*}
        p=${e##*:}
        l=${e#*:}
        l=${l%%:*}
        in_list "$r" "$routines" && in_list "$l" "$levels" && in_list "$p" "$precisions" || continue
        if [ ! -x "$(bench "$r" "$l" "$p")" ]; then
            in_list "$r" "$routines" && [ -n "$routines" ] &&
                refuse "--routine: $r is not in $suite"
            missing="$missing $r"
            continue
        fi
        selected="$selected $e"
        count=$((count + 1))
    done
    [ "$count" -gt 0 ] || refuse "no benchmark of $suite is selected"
}

# backend: the backend the suite was built for, from the --version of its
# first benchmark selected; every result it writes must record it.
backend() {
    e=${selected# }
    e=${e%% *}
    l=${e#*:}
    "$(bench "${e%%:*}" "${l%%:*}" "${e##*:}")" --version </dev/null 2>/dev/null |
        sed -n 's/^BLAS backend: \([^ ]*\).*/\1/p'
}

# Every suite is checked before anything runs: a line a suite, an empty
# one for bmb_suite's own.
specs=${specs%"$nl"}
total=0
while IFS= read -r spec; do
    resolve "$spec" ||
        refuse "--blas: no blas-microbenchmark in ${spec#*=}: neither an install prefix, a build tree nor the directory of its levels"
    choose
    total=$((total + count))
    if [ -n "$name" ] && [ -z "$(backend)" ]; then
        refuse "--blas: $name: the backend of $suite cannot be read from its benchmarks' --version"
    fi
done <<SPECS
$specs
SPECS

if [ -e "$out" ] && [ ! -d "$out" ]; then
    refuse "$out is not a directory"
fi
if [ -d "$out" ] && [ -n "$(ls -A "$out")" ]; then
    refuse "$out already holds files, and the results of two runs would mix in the report: give another directory"
fi

i=0
failed=
while IFS= read -r spec; do
    resolve "$spec"
    choose
    dir=$out${name:+/$name}
    who=${name:+$name: }
    expected=
    [ -n "$name" ] && expected=$(backend)
    if [ "$dry" = 0 ]; then
        mkdir -p "$dir" || refuse "$dir cannot be created"
    fi
    [ -n "$missing" ] && say "${who}not in $suite, left out:$missing"
    for e in $selected; do
        r=${e%%:*}
        p=${e##*:}
        l=${e#*:}
        l=${l%%:*}
        i=$((i + 1))
        b=$(bench "$r" "$l" "$p")
        opts=$(profile_options "$r" "$l")
        # The label, then -f and -o, last: the benchmark takes the last of
        # an option given twice.
        if [ "$dry" = 1 ]; then
            # shellcheck disable=SC2086 # the profile's options, a word each
            c=$(quoted "$b" $opts "$@" ${name:+--label "$name"} -f json -o "$dir/$r.json")
            printf '%s\n' "${c# }"
            continue
        fi
        say "$who$r ($i/$total)"
        # shellcheck disable=SC2086 # the profile's options, a word each
        "$b" $opts "$@" ${name:+--label "$name"} -f json -o "$dir/$r.json" </dev/null >"$dir/$r.log" 2>&1
        rc=$?
        case $rc in
        0)
            # A result of another library would be a series under the
            # wrong name.
            if [ -n "$expected" ] && ! grep -qxF "  \"backend\": \"$expected\"," "$dir/$r.json"; then
                got=$(sed -n 's/^  "backend": "\(.*\)",$/\1/p' "$dir/$r.json")
                say "$who$r records the backend ${got:-none}, not $expected, which $suite was built for"
                failed="$failed $name/$r"
            fi
            ;;
        77) say "$who$r: skipped, $(tail -n 1 "$dir/$r.log")" ;;
        *)
            say "$who$r failed (exit $rc); the end of $dir/$r.log:"
            tail -n 5 "$dir/$r.log" | sed 's/^/    /' >&2
            failed="$failed ${name:+$name/}$r"
            ;;
        esac
    done
done <<SPECS
$specs
SPECS

if [ -n "$failed" ]; then
    say "failed:$failed"
    exit 1
fi
exit 0
