#!/bin/sh
# Checks the skills (#263), skills/<name>/SKILL.md, which tell an agent
# how to measure, compare and read results as the project would: each has
# the front matter of the Agent Skills format (a name, its directory's,
# and a description of when to use it), every command in its code blocks
# runs a program that exists with options it takes, and every bmb_mcp
# tool it names is one bmb_mcp lists.
set -e
. "${srcdir:-.}/man_helper.sh"

S=${srcdir:-.}/../skills
n=0
: >"$T/commands"
: >"$T/named"
for f in "$S"/*/SKILL.md; do
    [ -f "$f" ] || continue
    dir=$(basename "$(dirname "$f")")
    test "$(sed -n 1p "$f")" = '---' || fail "$f: no front matter"
    sed -n '2,/^---$/p' "$f" >"$T/front"
    name=$(sed -n 's/^name: //p' "$T/front")
    test "$name" = "$dir" || fail "$f: named '$name', in the directory $dir"
    printf '%s\n' "$name" | grep -qE '^[a-z0-9]+(-[a-z0-9]+)*$' || fail "$f: '$name' is not lower case words joined by -"
    desc=$(sed -n 's/^description: //p' "$T/front")
    test -n "$desc" || fail "$f: no description"
    test "${#desc}" -le 1024 || fail "$f: a description of ${#desc} characters, past 1024"
    awk '/^```/ { on = !on; next } on' "$f" >>"$T/commands"
    grep -oE '`(list|describe|run|read|make)_[a-z_]+`' "$f" | tr -d '`' >>"$T/named" || true
    n=$((n + 1))
done
test "$n" -ge 3 || fail "only $n skills in $S"
ok "the $n skills have a name, their directory's, and a description"

check_commands "$T/commands"
# shellcheck disable=SC2154 # set by man_helper.sh's check_commands
test "$commands_checked" -ge 5 || fail "only $commands_checked commands found in the skills"
ok "the $commands_checked commands in the skills run programs that exist, with options they take"

if command -v python3 >/dev/null 2>&1; then
    echo '{"jsonrpc":"2.0","id":1,"method":"tools/list"}' | $MCP --results-dir "$T/mcp" >"$T/tools"
    grep -o '"name": "[a-z_]*"' "$T/tools" | sed 's/.*: "//; s/"$//' | sort -u >"$T/listed"
    sort -u "$T/named" >"$T/named.sorted"
    test -s "$T/named.sorted" || fail "the skills name no bmb_mcp tool"
    unknown=$(comm -23 "$T/named.sorted" "$T/listed")
    test -z "$unknown" || fail "the skills name tools bmb_mcp does not have: $unknown"
    ok "the $(wc -l <"$T/named.sorted") bmb_mcp tools the skills name are tools it lists"
else
    echo "note no python3 to run bmb_mcp: the tools the skills name are not checked"
fi
