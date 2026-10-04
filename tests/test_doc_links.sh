#!/bin/sh
# Checks every relative link in the Markdown files in git: the file or
# directory it points to is in git too, and so is the heading its #anchor
# names, spelt the way GitHub spells anchors. The developer guide is split across
# AGENTS.md and doc/dev/, and a section moved or renamed in one file
# breaks links in the others without a word.
#
# Links to the web are not followed. The issue and pull request templates
# are left out: GitHub resolves their links against the page they are
# shown on, not the file. Needs git to list the files, so it SKIPs outside
# a checkout, as test_editorconfig.sh does.
set -e

S=${srcdir:-.}

command -v git >/dev/null 2>&1 || { echo "SKIP: no git"; exit 77; }
test -e "$S/.git" || { echo "SKIP: $S is not a git checkout"; exit 77; }
files=$(git -C "$S" ls-files '*.md' ':!:.github/ISSUE_TEMPLATE/*' ':!:.github/pull_request_template.md')
test -n "$files" || { echo "FAIL: git lists no Markdown files in $S"; exit 1; }

cd "$S"
# What a link may lead to: every file in git, and every directory above one.
tracked=$(mktemp)
trap 'rm -f "$tracked"' EXIT
git ls-files >"$tracked"

# shellcheck disable=SC2086
LC_ALL=C awk -v tracked="$tracked" '
# GitHub: lower case, drop everything but letters, digits, spaces, - and _,
# then each space becomes a -. A heading seen before gets -1, -2, ...
function slug(h,    s) {
    s = tolower(h)
    gsub(/[^a-z0-9 _-]/, "", s)
    gsub(/ /, "-", s)
    return s
}

# The directory part of a path, "" for a top-level file.
function dir(p) {
    return (p ~ /\//) ? substr(p, 1, match(p, /\/[^\/]*$/)) : ""
}

# a/b/../c -> a/c
function normal(p,    n, parts, out, i, k) {
    n = split(p, parts, "/")
    k = 0
    for (i = 1; i <= n; i++) {
        if (parts[i] == "." || parts[i] == "") continue
        if (parts[i] == ".." && k > 0) { k--; continue }
        out[++k] = parts[i]
    }
    p = ""
    for (i = 1; i <= k; i++) p = p (i > 1 ? "/" : "") out[i]
    return p
}

function anchors(f,    line, fence, h, s) {
    if (f in loaded) return
    loaded[f] = 1
    fence = 0
    while ((getline line < f) > 0) {
        if (line ~ /^ *```/) { fence = !fence; continue }
        if (fence || line !~ /^#+ /) continue
        h = line
        sub(/^#+ +/, "", h)
        sub(/ +#* *$/, "", h)
        s = slug(h)
        if ((f, s) in anchor) { seen[f, s]++; s = s "-" seen[f, s] }
        anchor[f, s] = 1
    }
    close(f)
}

BEGIN {
    while ((getline t < tracked) > 0) {
        known[t] = 1
        while (t ~ /\//) {
            sub(/\/[^\/]*$/, "", t)
            known[t] = 1
        }
    }
    close(tracked)
}

function check(f, n, target,    path, frag, t) {
    if (target ~ /^[a-z]+:/ || target ~ /^</) return
    path = target
    frag = ""
    if (index(path, "#")) {
        frag = substr(path, index(path, "#") + 1)
        path = substr(path, 1, index(path, "#") - 1)
    }
    t = (path == "") ? f : normal(dir(f) path)
    checked++
    if (!(t in known)) {
        printf "FAIL: %s:%d: %s: not in git\n", f, n, target
        bad++
        return
    }
    if (frag == "") return
    if (t !~ /\.md$/) {
        printf "FAIL: %s:%d: %s: an anchor into a file that is not Markdown\n", f, n, target
        bad++
        return
    }
    anchors(t)
    if (!((t, frag) in anchor)) {
        printf "FAIL: %s:%d: %s: no heading #%s in %s\n", f, n, target, frag, t
        bad++
    }
}

{
    if (FNR == 1) fence = 0
    if ($0 ~ /^ *```/) { fence = !fence; next }
    if (fence) next
    line = $0
    # Inline code is not a link: `x](y)` in a code span stays text.
    gsub(/`[^`]*`/, "", line)
    while (match(line, /\]\([^)[:space:]]+\)/)) {
        # check() runs match() too, which resets RSTART and RLENGTH.
        target = substr(line, RSTART + 2, RLENGTH - 3)
        line = substr(line, RSTART + RLENGTH)
        check(FILENAME, FNR, target)
    }
    if (match(line, /^\[[^]]+\]: +[^[:space:]]+/)) {
        t = line
        sub(/^\[[^]]+\]: +/, "", t)
        sub(/[[:space:]].*/, "", t)
        check(FILENAME, FNR, t)
    }
}

END {
    if (checked < 30) { printf "FAIL: only %d relative links found\n", checked; exit 1 }
    if (bad) { printf "FAIL: %d broken link(s)\n", bad; exit 1 }
    printf "ok   all %d relative links in %d Markdown files lead to a file and a heading\n", checked, ARGC - 1
}
' $files
echo "all checks passed"
