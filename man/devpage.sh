# Sourced by html.sh. development_body TOP writes the body of the site's
# development.html -- the tests, and what CI runs -- to standard output.
#
# TOP is the top of the source tree. Nothing on that page is written by
# hand, so it cannot fall behind: each test is described by the first
# paragraph of its own header comment, and each CI job by what its
# workflow file says -- the comment above it, where it runs, its configure
# line, its steps. The workflows are not in a release tarball (.github is
# not distributed); without them the page has the tests only.
#
# The helpers it uses (heading, escape_html) are html.sh's.

# The first paragraph of a file's header comment, as one line: the "# ..."
# lines after any #!, or a /* ... */ block.
summary() {
    awk 'NR == 1 && /^#!/ { next }
         NR <= 2 && /^\/\*/ { c = 1; sub(/^\/\* ?/, "") }
         c && /^ \*\/?$/ { exit }
         c && /\*\// { sub(/ *\*\/.*/, ""); if ($0 != "") print; exit }
         c { sub(/^ \* ?/, ""); print; next }
         /^# ?$/ { exit }
         /^#/ { sub(/^# ?/, ""); print; next }
         { exit }' "$1" | tr '\n' ' ' | sed 's/ *$//'
}

# The per-routine smoke test of a benchmark that exists: test_bmb_ddot.sh,
# next to bmb_ddot.c.
is_smoke() {
    n=$(basename "$1" .sh)
    [ -f "$(dirname "$1")/bmb_${n#test_bmb_}.c" ]
}

# The conventions of the source comments, made HTML: `code`, *emphasis*
# (a * between spaces, as in "2 * 8", is left alone), and " -- " for a dash.
prose() {
    escape_html | sed -e 's/`\([^`]*\)`/<code>\1<\/code>/g' \
                      -e 's/\*\([^* ][^*]*[^* ]\)\*/<em>\1<\/em>/g' \
                      -e 's/ -- / \&mdash; /g'
}

# test_entry FILE: a test, by name, described by its header comment.
test_entry() {
    echo "  <dt><code>$(basename "$1")</code> <span class=\"dt-where\">$(dirname "${1#"$top"/}")</span></dt>"
    echo "  <dd>$(summary "$1" | prose)</dd>"
}

# workflow FILE: the workflow's header comment, then each of its jobs.
workflow() {
    awk '
    function esc(s) { gsub(/&/, "\\&amp;", s); gsub(/</, "\\&lt;", s); gsub(/>/, "\\&gt;", s); return s }
    function flush() {
        if (job == "") return
        where = esc(runs)
        if (container != "") where = where ", in " esc(container)
        if (matrix != "") where = where ", once for each of " esc(matrix)
        printf "  <dt><code>%s</code> <span class=\"dt-where\">%s</span></dt>\n  <dd>", job, where
        if (comment != "") printf "<p class=\"Pp\">%s</p>", esc(comment)
        if (configure != "") printf "<p class=\"Pp\"><code>%s</code></p>", esc(configure)
        if (steps != "") printf "<p class=\"Pp steps\">%s</p>", steps
        print "</dd>"
        job = runs = container = matrix = configure = steps = comment = ""
    }
    /^jobs:/ { injobs = 1; if (head != "") printf "<p class=\"Pp\">%s</p>\n", esc(head); print "<dl class=\"Bl-tag\">"; next }
    !injobs && /^# ?/ { h = $0; sub(/^# ?/, "", h); if (h != "") head = head (head == "" ? "" : " ") h; next }
    !injobs { next }
    /^  # ?/ { c = $0; sub(/^  # ?/, "", c); pending = pending (pending == "" ? "" : " ") c; next }
    /^  [A-Za-z0-9_-]+:$/ { flush(); job = $0; sub(/^ */, "", job); sub(/:$/, "", job); comment = pending; pending = ""; next }
    /^    runs-on: / { runs = $0; sub(/^ *runs-on: */, "", runs); next }
    /^    container: / { container = $0; sub(/^ *container: */, "", container); next }
    /^        [a-z]+: \[/ { matrix = $0; sub(/^[^[]*\[/, "", matrix); sub(/\].*/, "", matrix); next }
    # A configure line, joined with its continuation lines.
    cont { l = $0; sub(/^ */, "", l); configure = configure " " l; cont = (configure ~ /\\$/); if (!cont) sub(/ *\\$/, "", configure); gsub(/ *\\ +/, " ", configure); next }
    /\.\.\/configure/ && configure == "" { configure = $0; sub(/^ */, "", configure); cont = (configure ~ /\\$/); next }
    # A step name; "${{ matrix.browser }}" reads as "<i>browser</i>".
    /^      - name: / { s = $0; sub(/^ *- name: */, "", s); s = esc(s)
                        while (match(s, /\$\{\{ *matrix\.[a-z_]+ *\}\}/)) { v = substr(s, RSTART, RLENGTH); sub(/^\$\{\{ *matrix\./, "", v); sub(/ *\}\}$/, "", v); s = substr(s, 1, RSTART - 1) "<i>" v "</i>" substr(s, RSTART + RLENGTH) }
                        steps = steps (steps == "" ? "" : " &rarr; ") s; next }
    END { flush(); print "</dl>" }' "$1"
}

development_body() {
    top=$1

    echo '<section class="Sh">'
    heading 1 tests "Tests"
    echo '<p class="Pp"><code>make check</code> runs all of them. Each is a program or a script that exits 0 to pass, 77 to report SKIP when something it needs is missing (a browser, groff, mandoc), and anything else to fail. Each says what it checks in the words of its own source.</p>'

    heading 2 tests-benchmarks "The benchmarks"
    echo '<dl class="Bl-tag">'
    for f in "$top"/src/c/common/test_*.c "$top"/src/c/netlib/test_*.c "$top"/src/c/test_*.sh; do
        [ -f "$f" ] && test_entry "$f"
    done
    smoke=0
    for f in "$top"/src/c/level*/test_bmb_*.sh; do
        if is_smoke "$f"; then
            smoke=$((smoke + 1))
        else
            test_entry "$f"
        fi
    done
    echo "  <dt><code>test_bmb_&lt;routine&gt;.sh</code> <span class=\"dt-where\">src/c/level1&ndash;3, $smoke of them</span></dt>"
    echo "  <dd>One per benchmark: each runs its benchmark on tiny sizes, and the assertions of <code>src/c/test_helper.sh</code> check what it printed.</dd>"
    echo '</dl>'

    heading 2 tests-report "The report"
    echo '<dl class="Bl-tag">'
    for f in "$top"/src/report/test_*.sh "$top"/src/report/test_report.js; do
        [ -f "$f" ] && test_entry "$f"
    done
    echo '</dl>'

    heading 2 tests-project "The project files"
    echo '<dl class="Bl-tag">'
    for f in "$top"/test_*.sh; do
        [ -f "$f" ] && test_entry "$f"
    done
    echo '</dl>'

    heading 2 tests-man "The manual pages and this site"
    echo '<dl class="Bl-tag">'
    for f in "$top"/man/test_man_*.sh; do
        [ -f "$f" ] && test_entry "$f"
    done
    echo '</dl>'
    echo '</section>'

    [ -d "$top/.github/workflows" ] || return 0
    echo '<section class="Sh">'
    heading 1 ci "Continuous integration"
    echo '<p class="Pp">GitHub Actions runs four workflows: CI and CodeQL on every pull request and push to <code>main</code> (CodeQL weekly as well), the site on every push to <code>main</code>, the release on a version tag and, publishing nothing, on a pull request that changes it. Each job is described by its workflow file: the comment above it, where it runs, how it configures the build, and its steps in order.</p>'
    for w in ci codeql pages release; do
        f=$top/.github/workflows/$w.yml
        [ -f "$f" ] || continue
        heading 2 "ci-$w" "$(sed -n 's/^name: *//p' "$f" | head -1)"
        echo "<p class=\"Pp where\"><code>.github/workflows/$w.yml</code></p>"
        workflow "$f"
    done
    echo '</section>'
}
