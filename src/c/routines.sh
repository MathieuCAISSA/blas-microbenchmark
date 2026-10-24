#!/bin/sh
# Writes every list of the routines from src/c/routines.txt into the
# files that hold one, between their "BEGIN routines (KIND)" and "END
# routines (KIND)" lines, KIND naming the list, so that a routine is added
# in one place (#199):
#
#     src/c/routines.sh [--check] [TOP]
#
# TOP is the top of the source tree (the current directory by default).
# With --check it changes nothing, and fails with the difference when a
# list is not what the table gives: make check runs it that way
# (tests/test_routines.sh). ./autogen.sh runs it without, before
# autoreconf, so that the Makefile.am it writes are the ones Automake
# reads.
set -e

check=0
if [ "$1" = --check ]; then
    check=1
    shift
fi
top=${1:-.}
table=$top/src/c/routines.txt

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# block KIND: the lines KIND's list is, from the table. The table is read
# in its order; a malformed line, or a routine listed twice, stops it.
block() {
    awk -v kind="$1" '
        BEGIN {
            dir["s"] = "single"; dir["d"] = "double"
            dir["c"] = "complex"; dir["z"] = "complex16"
            size["mn"] = "\\-m is M, \\-M is N"
            size["mkn"] = "\\-m is M and K, \\-M is N"
            size["nk"] = "\\-m is N, \\-M is K"
            size["n"] = "\\-m is N (square; no \\-M)"
            size["v"] = ""
        }
        /^#/ || NF == 0 { next }
        {
            if (NF != 5 || $2 !~ /^[123]$/ || !($3 in dir) || !($4 in size) ||
                $1 !~ /^[a-z][a-z0-9]*$/ || $5 !~ /^[a-z][a-z0-9]*$/) {
                printf "routines.txt:%d: not \"routine level precision sizes family\": %s\n", NR, $0 >"/dev/stderr"
                bad = 1
                exit 1
            }
            if ($1 in seen) {
                printf "routines.txt:%d: %s listed twice\n", NR, $1 >"/dev/stderr"
                bad = 1
                exit 1
            }
            seen[$1] = 1
            n++
            name[n] = $1; level[n] = $2; prec[n] = $3; sizes[n] = $4; family[n] = $5
        }
        # wrap: the words of list, as lines of at most width bytes once
        # first (the first line) or rest (the others) is put before them,
        # each line but the last ended with end.
        function wrap(list, width, first, rest, end,    w, k, i, line, out) {
            k = split(list, w, " ")
            line = first
            for (i = 1; i <= k; i++) {
                if (line != first && line != rest && length(line " " w[i] end) > width) {
                    out = out line end "\n"
                    line = rest
                }
                line = (line == first || line == rest) ? line w[i] : line " " w[i]
            }
            return out line "\n"
        }
        END {
            if (bad) { exit 1 }
            if (kind ~ /^make[123]$/) {
                l = substr(kind, 5)
                printf "level%s_PROGRAMS =", l
                for (i = 1; i <= n; i++) {
                    if (level[i] == l) { printf " \\\n\t%s/bmb_%s", dir[prec[i]], name[i] }
                }
                printf "\n\n"
                # A family source is compiled once per precision, with
                # flags of its own, so that each program has its object.
                for (i = 1; i <= n; i++) {
                    if (level[i] != l) { continue }
                    p = dir[prec[i]] "_bmb_" name[i]
                    printf "%s_SOURCES = bmb_%s.c\n", p, family[i]
                    printf "%s_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_%s\n", p, toupper(prec[i])
                }
                printf "\nROUTINE_TESTS ="
                for (i = 1; i <= n; i++) {
                    if (level[i] == l) { printf " \\\n\t%s/test_bmb_%s.sh", dir[prec[i]], name[i] }
                }
                printf "\n"
            } else if (kind == "man-makefile") {
                for (i = 1; i <= n; i++) { all = all " " name[i] }
                printf "%s", wrap(substr(all, 2), 72, "ROUTINES = ", "\t", " \\")
            } else if (kind == "man-name") {
                # The last name and what follows it on one line, as
                # makewhatis reads it.
                list = "blas-microbenchmark,"
                for (i = 1; i < n; i++) { list = list " bmb_" name[i] "," }
                t = wrap(list " bmb_" name[n] "\001\\-\001microbenchmarks\001for\001BLAS\001routines", 76, "", "", "")
                gsub(/\001/, " ", t)
                printf "%s", t
            } else if (kind == "man-routines") {
                printf ".TS\nl l.\n"
                for (l = 1; l <= 3; l++) {
                    list = ""
                    for (i = 1; i <= n; i++) {
                        if (level[i] == l) { list = list " " name[i] }
                    }
                    printf "%s", wrap(substr(list, 2), 72, "Level " l "\t", "\\&\t", "")
                }
                printf ".TE\n"
            } else if (kind == "man-sizes") {
                printf ".TS\nl l.\n"
                k = split("mn mkn nk n", order, " ")
                for (j = 1; j <= k; j++) {
                    list = ""
                    for (i = 1; i <= n; i++) {
                        if (sizes[i] == order[j]) { list = list " " name[i] }
                    }
                    if (list == "") { continue }
                    t = wrap(substr(list, 2), 34, "", "", "")
                    gsub(/\n/, "\t" size[order[j]] "\n", t)
                    printf "%s", t
                }
                printf ".TE\n"
            } else if (kind == "readme") {
                print "| Level | Double | Single | Complex |"
                print "| --- | --- | --- | --- |"
                split("vectors|matrix × vector|matrix × matrix", what, "|")
                for (l = 1; l <= 3; l++) {
                    d = ""; s = ""; c = ""
                    for (i = 1; i <= n; i++) {
                        if (level[i] != l) { continue }
                        cell = " `" name[i] "`"
                        if (prec[i] == "d") { d = d cell }
                        else if (prec[i] == "s") { s = s cell }
                        else { c = c cell }
                    }
                    printf "| **%d** — %s |%s |%s |%s |\n", l, what[l], d, s, c
                }
            } else if (kind == "report" || kind == "reframe") {
                # A line per level at least; \001 keeps a name and its
                # level on one line.
                q = kind == "reframe" ? "\047" : ""
                t = ""
                for (l = 1; l <= 3; l++) {
                    list = ""
                    for (i = 1; i <= n; i++) {
                        if (level[i] == l) { list = list " " q name[i] q ":\001" l "," }
                    }
                    if (list != "") { t = t wrap(substr(list, 2), 79, "    ", "    ", "") }
                }
                gsub(/\001/, " ", t)
                if (kind == "report") {
                    sub(/,\n$/, "\n", t)
                    printf "  var LEVEL = {\n%s  };\n", t
                } else {
                    printf "ROUTINES = {\n%s}\n", t
                }
            }
        }
    ' "$table"
}

# update FILE KIND: FILE with its list replaced by KIND's.
status=0
update() {
    f=$top/$1
    block "$2" >"$tmp/block"
    awk -v block="$tmp/block" -v file="$1" -v kind="$2" '
        index($0, "BEGIN routines (" kind ")") {
            print
            while ((getline line <block) > 0) { print line }
            begins++
            skip = 1
            next
        }
        index($0, "END routines (" kind ")") { skip = 0; ends++ }
        !skip { print }
        END {
            if (begins != 1 || ends != 1) {
                printf "%s: needs one BEGIN and one END routines (%s) line\n", file, kind >"/dev/stderr"
                exit 1
            }
        }
    ' "$f" >"$tmp/new"
    if cmp -s "$f" "$tmp/new"; then
        return 0
    fi
    if [ "$check" = 1 ]; then
        echo "$1: its list of routines is not the table's; run src/c/routines.sh"
        diff -u "$f" "$tmp/new" || :
        status=1
    else
        cat "$tmp/new" >"$f"
        echo "routines.sh: updated $1"
    fi
}

update src/c/level1/Makefile.am make1
update src/c/level2/Makefile.am make2
update src/c/level3/Makefile.am make3
update man/Makefile.am man-makefile
update man/blas-microbenchmark.1.in man-name
update man/blas-microbenchmark.1.in man-routines
update man/blas-microbenchmark.1.in man-sizes
update README.md readme
update src/report/report.html report
update packaging/reframe/blas_microbenchmark.py reframe
exit "$status"
