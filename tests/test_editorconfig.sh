#!/bin/sh
# Checks every file in git against .editorconfig, so that what it promises
# an editor is also true of the files already there: UTF-8, LF line ends,
# a final newline, no trailing whitespace, and indentation with spaces --
# a tab first in Makefile.am, where make needs it (spaces may follow, to
# align a continuation line).
#
# indent_size is for the editor only: continuation lines are aligned with
# what they continue, at any width.
#
# Needs git to list the files, so it SKIPs in a build from the tarball,
# which is made from a tree this test has passed.
set -e

S=${srcdir:-.}

fail() {
    echo "FAIL: $*"
    exit 1
}

ok() {
    echo "ok   $*"
}

# The settings checked below are the ones .editorconfig declares.
for line in 'root = true' '[*]' 'charset = utf-8' 'end_of_line = lf' \
    'insert_final_newline = true' 'trim_trailing_whitespace = true' \
    'indent_style = space' '[{Makefile.am,Makefile}]' 'indent_style = tab'; do
    grep -qxF "$line" "$S/.editorconfig" || fail ".editorconfig has no line '$line'"
done
ok ".editorconfig declares the settings this test checks"

command -v git >/dev/null 2>&1 || { echo "SKIP: no git"; exit 77; }
# A tarball unpacked inside a checkout (make distcheck does) is not one.
test -e "$S/.git" || { echo "SKIP: $S is not a git checkout"; exit 77; }
files=$(git -C "$S" ls-files 2>/dev/null) || { echo "SKIP: $S is not a git checkout"; exit 77; }
test -n "$files" || fail "git lists no files in $S"

tab=$(printf '\t')
cr=$(printf '\r')
bad=0
n=0

# Prints one line per problem, as file:line: what.
problem() {
    echo "FAIL: $1: $2"
    bad=$((bad + 1))
}

# The first lines of $1 matching $2, as problems named $3.
lines() {
    if LC_ALL=C grep -q "$2" "$S/$1"; then
        for l in $(LC_ALL=C grep -n "$2" "$S/$1" | head -3 | cut -d: -f1); do
            problem "$1:$l" "$3"
        done
    fi
}

while IFS= read -r f; do
    p=$S/$f
    test -f "$p" || continue
    test -s "$p" || continue
    # Binary files (the screenshots) are not text to format. A NUL byte
    # tells them apart: grep -I would also skip a text file that is not
    # UTF-8, which is one of the things to catch.
    LC_ALL=C tr -d '\000' <"$p" | cmp -s - "$p" || continue
    n=$((n + 1))

    if command -v iconv >/dev/null 2>&1 && ! iconv -f UTF-8 -t UTF-8 "$p" >/dev/null 2>&1; then
        problem "$f" "not UTF-8"
    fi
    test "$(tail -c 1 "$p" | od -An -c | tr -d ' ')" = '\n' || problem "$f" "no newline at the end"
    lines "$f" "$cr" "CRLF line end"
    lines "$f" "[ $tab]\$" "trailing whitespace"
    case ${f##*/} in
        Makefile.am | Makefile) lines "$f" "^ " "indented with spaces, make needs a tab first" ;;
        *) lines "$f" "^ *$tab" "indented with a tab" ;;
    esac
done <<EOF
$files
EOF

test "$n" -ge 100 || fail "only $n text files checked"
test "$bad" -eq 0 || fail "$bad problem(s) above; see .editorconfig"
ok "all $n text files in git follow .editorconfig"
echo "all checks passed"
