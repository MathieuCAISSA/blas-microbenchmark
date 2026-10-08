#!/bin/sh
# Captures the charts and tables shown in the README and on the
# documentation site from a report, in light and dark:
#
#     doc/screenshots.sh report.html doc/images [NAME...]
#
# writes <name>-light.png and <name>-dark.png for each chart in SHOTS below,
# or for the NAMEs given only: the repeated runs' images come from a report
# of their own (doc/dev/report.md),
# at twice the CSS resolution so they stay sharp on high-density screens.
# Each image is the chart's own element, not the window, so the page around
# it never ends up in the README.
#
# It drives Firefox -- the browser the README tells users to open reports
# with -- over WebDriver, so it needs firefox, geckodriver and curl.
# BMB_GECKODRIVER overrides the geckodriver command. The README says which
# commands produced the results these images come from; keep the two in
# step when the images are regenerated.
set -e

if [ $# -lt 2 ]; then
    echo "usage: $0 REPORT.html OUTDIR [NAME...]" >&2
    exit 2
fi
page="file://$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
out=$2
shift 2
mkdir -p "$out"

# name  window width  what  [routine  text in the chart's title]
#
# what is "chart" (a chart, found by its routine and title), "summary" (the
# tiles and the table of where the results came from) or "rawdata" (the
# raw data table, opened, its first rows fading out).
#
# The page lays charts out to the width they get. At 1200px the line charts
# sit two to a row, about 540px each; the heatmap spans the whole row, so it
# is taken at a width that gives it about the same size and proportions,
# and the README can show the four as a regular grid.
SHOTS='ddot-size 1200 chart ddot Performance against size
dgemm-ratio 1200 chart dgemm compared with
dgemm-threads 1200 chart dgemm Thread scaling
dgemv-shapes 640 chart dgemv Shapes
summary 1000 summary
rawdata 1000 rawdata
noise-band 1200 chart ddot Performance against size
noise-ratio 1200 chart ddot compared with'

# Without NAMEs, the README's report: every shot but the repeated runs'.
if [ $# -eq 0 ]; then
    SHOTS=$(printf '%s\n' "$SHOTS" | grep -v '^noise-')
else
    want=$(printf '%s\n' "$@")
    SHOTS=$(printf '%s\n' "$SHOTS" | while read -r name rest; do
        if printf '%s\n' "$want" | grep -qx "$name"; then
            printf '%s %s\n' "$name" "$rest"
        fi
    done)
    [ -n "$SHOTS" ] || { echo "$0: none of $* is in SHOTS" >&2; exit 2; }
fi

gd=${BMB_GECKODRIVER:-geckodriver}
port=$((20000 + $$ % 20000))
base=http://127.0.0.1:$port

fail() {
    echo "$0: $*" >&2
    [ -n "$gd_pid" ] && kill "$gd_pid" 2>/dev/null
    exit 1
}

wd() {
    if [ $# -ge 3 ]; then
        curl -s -X "$1" -H 'Content-Type: application/json' -d "$3" "$base$2"
    else
        curl -s -X "$1" "$base$2"
    fi
}

"$gd" --port "$port" --binary "$(command -v firefox)" >/dev/null 2>&1 &
gd_pid=$!
tries=0
until curl -s "$base/status" >/dev/null 2>&1; do
    tries=$((tries + 1))
    [ "$tries" -le 30 ] || fail "geckodriver did not start"
    sleep 1
done

# Finds the element to capture: arguments are what, then the routine and
# the title text for a chart. No double quotes in it, so it can sit in a
# JSON string as is.
FIND="if (arguments[0] === 'summary') {
  var s = document.querySelector('main section'); s.style.padding = '16px 16px 4px';
  s.scrollIntoView(); return s;
}
if (arguments[0] === 'rawdata') {
  var d = document.querySelector('main details.card');
  d.open = true;
  var w = d.querySelector('.table-wrap');
  w.style.maxHeight = '400px'; w.style.overflow = 'hidden';
  w.style.maskImage = 'linear-gradient(to bottom, black 70%, transparent)';
  d.scrollIntoView(); return d;
}
var secs = document.querySelectorAll('main section');
for (var i = 0; i < secs.length; i++) {
  var h = secs[i].querySelector('h2');
  if (!h || h.textContent !== arguments[1]) { continue; }
  var figs = secs[i].querySelectorAll('figure');
  for (var j = 0; j < figs.length; j++) {
    var t = figs[j].querySelector('h3');
    if (t && t.textContent.indexOf(arguments[2]) >= 0) { figs[j].scrollIntoView(); return figs[j]; }
  }
}
return null;"
FIND=$(printf '%s' "$FIND" | tr '\n' ' ')

for theme in light dark; do
    dark=0
    [ "$theme" = dark ] && dark=1
    resp=$(wd POST /session "{\"capabilities\":{\"alwaysMatch\":{\"moz:firefoxOptions\":{\"args\":[\"-headless\"],\"prefs\":{\"layout.css.devPixelsPerPx\":\"2\",\"ui.systemUsesDarkTheme\":$dark}}}}}")
    sid=$(printf '%s' "$resp" | sed -n 's/.*"sessionId":"\([^"]*\)".*/\1/p')
    [ -n "$sid" ] || fail "Firefox did not start: $resp"
    wd POST "/session/$sid/url" "{\"url\":\"$page\"}" >/dev/null

    printf '%s\n' "$SHOTS" | while read -r name width what routine title; do
        # The charts redraw on the resize event, which Firefox delivers
        # asynchronously: give it a moment.
        wd POST "/session/$sid/window/rect" "{\"width\":$width,\"height\":1600}" >/dev/null
        sleep 1
        el=$(wd POST "/session/$sid/execute/sync" "{\"script\":\"$FIND\",\"args\":[\"$what\",\"$routine\",\"$title\"]}" \
            | sed -n 's/.*"element-6066-11e4-a52e-4f735466cecf":"\([^"]*\)".*/\1/p')
        [ -n "$el" ] || fail "no $what${title:+ \"$title\"}${routine:+ for $routine} in $1"
        wd GET "/session/$sid/element/$el/screenshot" \
            | sed -n 's/.*"value":"\([^"]*\)".*/\1/p' | base64 -d >"$out/$name-$theme.png"
        test -s "$out/$name-$theme.png" || fail "empty screenshot for $name ($theme)"
        echo "$out/$name-$theme.png"
    done
    wd DELETE "/session/$sid" >/dev/null
done

kill "$gd_pid" 2>/dev/null
wait "$gd_pid" 2>/dev/null || true
