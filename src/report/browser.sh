# Sourced by the tests that run the page in a browser:
#
#     . "${srcdir:-.}/browser.sh"
#     find_browser                   # sets $browser, or exits 77 (SKIP)
#     render_dom page.html dom.html  # the DOM once the page's scripts ran
#
# Two kinds of browser, because they are driven differently:
#  - Chromium-based ones -- Chrome, Chromium, Edge -- which print the
#    rendered DOM with --dump-dom;
#  - Firefox, which has no such flag, so it is driven over WebDriver by
#    geckodriver, with curl. Firefox is the browser the README tells users
#    to open the page with, so it is the one that matters most.
#
# BMB_BROWSER picks one (a command or a path; anything whose name contains
# "firefox" is driven as Firefox, anything else as Chromium). Otherwise
# Chrome, Chromium or Edge is used if found, then Firefox if geckodriver
# and curl are there too. BMB_GECKODRIVER overrides
# the geckodriver command.
#
# Expects a fail() function from the test that sources it.

find_browser() {
    browser=${BMB_BROWSER:-}
    if [ -z "$browser" ]; then
        for b in google-chrome google-chrome-stable chromium chromium-browser \
                 microsoft-edge microsoft-edge-stable; do
            if command -v "$b" >/dev/null 2>&1; then
                browser=$b
                break
            fi
        done
    fi
    if [ -z "$browser" ] && command -v firefox >/dev/null 2>&1 \
        && command -v "${BMB_GECKODRIVER:-geckodriver}" >/dev/null 2>&1 \
        && command -v curl >/dev/null 2>&1; then
        browser=firefox
    fi
    if [ -z "$browser" ]; then
        echo "SKIP: no browser to run the page in (Chrome, Chromium, Edge, or Firefox with geckodriver and curl)"
        exit 77
    fi
    echo "     rendering with $browser"
}

# render_dom PAGE OUT: writes to OUT the serialised DOM of PAGE, a file,
# after its scripts have run.
render_dom() {
    case $browser in
        *firefox*) render_dom_firefox "$1" "$2" ;;
        *) render_dom_chrome "$1" "$2" ;;
    esac
    test -s "$2" || fail "the browser returned an empty DOM"
}

# Headless without a sandbox: the page is our own, read from disk, and the
# user-namespace sandbox is often unavailable on CI.
render_dom_chrome() {
    "$browser" --headless=new --no-sandbox --disable-gpu --dump-dom \
        "file://$(cd "$(dirname "$1")" && pwd)/$(basename "$1")" >"$2" 2>"$2.log" \
        || fail "the browser did not render the page: $(cat "$2.log")"
}

render_dom_firefox() {
    page="file://$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
    gd=${BMB_GECKODRIVER:-geckodriver}
    # Each test runs its own geckodriver, and make check may run tests in
    # parallel: a port per process.
    port=$((20000 + $$ % 20000))
    base=http://127.0.0.1:$port

    firefox_bin=$browser
    case $firefox_bin in
        */*) ;;
        *) firefox_bin=$(command -v "$firefox_bin") || fail "$browser not found" ;;
    esac
    "$gd" --port "$port" --binary "$firefox_bin" >"$2.log" 2>&1 &
    gd_pid=$!
    tries=0
    until curl -s "$base/status" >/dev/null 2>&1; do
        tries=$((tries + 1))
        if [ "$tries" -gt 30 ]; then
            kill "$gd_pid" 2>/dev/null
            fail "geckodriver did not start: $(cat "$2.log")"
        fi
        sleep 1
    done

    wd() {
        if [ $# -ge 3 ]; then
            curl -s -X "$1" -H 'Content-Type: application/json' -d "$3" "$base$2"
        else
            curl -s -X "$1" "$base$2"
        fi
    }
    resp=$(wd POST /session '{"capabilities":{"alwaysMatch":{"moz:firefoxOptions":{"args":["-headless"]}}}}')
    sid=$(printf '%s' "$resp" | sed -n 's/.*"sessionId":"\([^"]*\)".*/\1/p')
    if [ -z "$sid" ]; then
        kill "$gd_pid" 2>/dev/null
        fail "Firefox did not start: $resp $(cat "$2.log")"
    fi
    # Navigation returns once the page has loaded, by which time its
    # scripts -- inline, at the end of <body> -- have run.
    wd POST "/session/$sid/url" "{\"url\":\"$page\"}" >"$2.nav"
    wd POST "/session/$sid/execute/sync" \
        '{"script":"return document.documentElement.outerHTML","args":[]}' >"$2.json"
    wd DELETE "/session/$sid" >/dev/null
    kill "$gd_pid" 2>/dev/null
    wait "$gd_pid" 2>/dev/null || true

    if grep -q '"error"' "$2.nav" "$2.json"; then
        fail "Firefox could not render the page: $(cat "$2.nav" "$2.json")"
    fi
    json_value "$2.json" >"$2"
}

# The reply is {"value":"<html>..."}, the DOM as one JSON string. Decodes
# the escapes a JSON encoder uses for HTML -- quotes, backslashes, control
# characters, and the \u forms some encoders give < > & ' -- which is all
# the checks need; this is not a general JSON parser. The backslash pairs
# go first, through a placeholder, so "\\n" stays a backslash and an n.
json_value() {
    awk '{
        s = s $0
    }
    END {
        sub(/^[^:]*:[[:space:]]*"/, "", s)
        sub(/"[[:space:]]*}[[:space:]]*$/, "", s)
        gsub(/\\\\/, "\001", s)
        gsub(/\\"/, "\"", s)
        gsub(/\\n/, "\n", s)
        gsub(/\\t/, "\t", s)
        gsub(/\\r/, "", s)
        gsub(/\\\//, "/", s)
        gsub(/\\u003[cC]/, "<", s)
        gsub(/\\u003[eE]/, ">", s)
        gsub(/\\u0026/, "\\&", s)
        gsub(/\\u0027/, "'"'"'", s)
        gsub(/\001/, "\\", s)
        printf "%s", s
    }' "$1"
}
