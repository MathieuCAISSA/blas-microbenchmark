# The report

How `bmb_report` and its page are built and tested. Start at
[AGENTS.md](../../AGENTS.md).

`bmb_report results/*.json > report.html` turns any number of JSON results
into one HTML page of charts (#1). The page must open from `file://` with
no network, so everything is inside it: data, styles, script, nothing
fetched. `test_bmb_report.sh` enforces that, among other things, by
refusing any external `src`/`href`.

**How it is built.** `src/report/bmb_report.sh` is a POSIX sh script with
two marker lines; `make` runs `assemble.awk`, which splits `report.html` at
its `<!-- @BMB_RESULTS@ -->` line and puts each half in place of a marker,
inside a *quoted* here-document. The template is therefore copied byte for
byte, with no shell expansion at all — and must never contain a line equal
to `BMB_REPORT_HEAD` or `BMB_REPORT_TAIL`, which would end the
here-document early (`assemble.awk` refuses to build if it does). Edit
`report.html` freely otherwise; re-run `make` and the installed command
picks it up.

**What it accepts** is decision 5 of #1: results of the same major version
as the report itself, which `assemble.awk` takes from `PACKAGE_VERSION`.
Everything else is refused *before* anything is written, with the file name
and the reason, because a page silently missing one backend would be read
as complete. That is also why `main` carries a `-dev` version between
releases (`1.2.0-dev` after 1.1.0): its own results have to be accepted by its own
report. `src/report/fixtures/` holds files in each refused format, laid out
exactly as those versions wrote them — taken from the git history, not
from memory. Current-format input is never a fixture: the test generates it
by running the benchmarks, so it cannot fall out of date.

The script checks files line by line rather than parsing JSON. That holds
because the benchmarks write them — one field per line, fixed indentation —
so any change to the JSON layout in `bmb_print.c` has to keep
`check_file()` in step.

Data goes into the page as one `<script type="application/json">` block per
file, with every `</` turned into `<\/` (the same character to JSON) so
that a `</script>` inside a label cannot close its element. Each block is
parsed on its own, so one bad file cannot take the others down with it.

## The page

All of it is in `report.html`: plain JavaScript and SVG, no library, in
keeping with "nothing fetched". The decisions it implements are #1's, and
the code points at them; the ones easiest to break by accident:

- **A series is routine + backend + BLAS string + CPU + label.** Files
  sharing one merge; a duplicate point keeps the fastest and is reported.
  The frequency governor and turbo are shown, not part of the key, so
  that older files still merge (see
  [backends.md](backends.md)).
- **Colours are assigned once per page**, in a fixed order with OpenBLAS
  first, so a series keeps its colour in every chart and under every
  selector. Never assign them per chart or by rank. The palette is the
  dataviz skill's reference palette, light and dark, used as validated —
  if you change a hue, re-run its validator rather than eyeballing it.
- **One y axis per chart.** GB/s is a second chart, not a second scale.
- **Anything from a result file reaches the DOM through `textContent`**,
  never `innerHTML`: a label is whatever someone typed.
- **Charts draw at the width they get** and redraw on resize, rather than
  scaling an SVG `viewBox`, so text stays at its real size.

## Testing the page

Two tests run the page in a real browser, through `browser.sh`, which both
source:

- `test_bmb_report_render.sh` generates input meant to trigger every chart
  and checks each one drew.
- `test_bmb_report_js.sh` runs `test_report.js`, the unit tests of the
  page's logic, which pin #1's decisions one check each (merging, fastest
  duplicate, names, colour slots, the reference, default threads, slices,
  ratios, thread scaling, the heatmap, cache markers, formatting).

**The model and the view are split for that.** Everything in `report.html`
from `buildModel` to the line before `window.bmbReport` is pure — it takes
results and returns numbers, never touches the DOM — and is exported on
`window.bmbReport`. The `...Chart` functions only draw what the matching
`...Data` function computed. Keep it that way: a decision made inside a
drawing function cannot be unit-tested, only eyeballed. The harness pastes
`test_report.js` after the page's script in a real report, so the tests
exercise the shipped code; the results come back in a `<pre
id="bmb-test-results">` read from the DOM. `test_report.js` must not
contain `</` (it would close the `<script>` it is pasted into), and the
harness refuses it if it does.

**Browsers.** The Chromium-based ones — Chrome, Chromium, Edge — print
the rendered DOM with `--dump-dom`. Firefox has no such flag, so
`browser.sh` drives it over WebDriver: geckodriver plus `curl`, and a
small awk decoder for the JSON string the DOM comes back in. Left to
itself, `browser.sh` takes the first of Chrome, Chromium and Edge it finds,
then Firefox if geckodriver is there too; with none, the tests SKIP.
`BMB_BROWSER=…` picks one (`firefox`, `microsoft-edge`, a path…);
`BMB_GECKODRIVER=…` points at a geckodriver that is not on `$PATH`.

| Browser | CI | Locally |
|---|---|---|
| Chrome | the x86 jobs (`openblas`, `blis`, `netlib`, `sanitizers`) | found on its own |
| Firefox | `browsers` job | `BMB_BROWSER=firefox make check` |
| Edge | `browsers` job | `BMB_BROWSER=microsoft-edge make check` |

The `openblas` and `browsers` jobs fail if the tests skipped. Run them in
Firefox before a release: it is the browser the README tells users to
open the page with, and the one most likely to differ.

Under WSL, a browser installed on the Windows side does not count: it
cannot open the `file:///home/…` pages the tests write, and WSL cannot
reach the Windows `localhost` that geckodriver listens on. Install the
Linux packages inside WSL (Chrome's `.deb`; Firefox from Mozilla's APT
repository — not Ubuntu's snap, which geckodriver handles badly — and the
geckodriver release tarball).

Things that took false passes to learn, so keep them:

- **Only look inside what the page rendered.** The serialised DOM also
  holds the page's own script, whose source contains every chart title
  verbatim (and `test_report.js`'s own source): grepping the whole document
  passes even when nothing drew. The render test reads only `<main>`, and
  `test_report.js` never spells its results tag in a comment.
- **Extract with awk, not a `sed` range.** The page builds `<main>` in one
  go, so it opens and closes on one line, and a `sed` range only looks for
  its end from the next line — it ran on through the script to the end of
  the file. Likewise the results tag shares a line with whatever precedes
  it and with the first check: strip up to the tag, or a failing first
  check goes unseen.
- **Strip carriage returns** before matching: a Windows browser ends its
  lines with them, and `$` then never matches.

The render test was checked against a page whose script throws on its
first line, and against one that never draws the heatmap; the JS test
against a missing export, a syntax error, a throwing function, a failing
first check, and wrong decisions (slowest duplicate kept, OpenBLAS not
first, ratio ticks never pruned). All fail. To look at a page rather than
test it, render a screenshot — `chrome --headless=new --screenshot=out.png
--window-size=1280,3000 file://$PWD/report.html`, or `firefox --headless
--screenshot out.png file://$PWD/report.html` — and do look, in light and
dark: the tests prove the charts exist and the numbers behind them are
right, not that they are readable.

## The README's images

`doc/images/` holds the screenshots of a report shown by the README (the
four charts) and by the site (those, the summary and the raw data
table), each in light and dark (`<picture>` picks one by the reader's
theme). They are real results, made by the commands in the README's
report section, and `doc/screenshots.sh` captures them from the report
those produce. Run `bmb_report` from the directory holding `results/`,
so the file names the summary and the raw data show are
`results/...`, not the paths of your machine:

```bash
cd somewhere && bmb_report results/*.json > report.html
doc/screenshots.sh report.html path/to/doc/images
```

It drives Firefox over WebDriver and screenshots each element on its own
at twice the CSS resolution, so nothing around it gets in; it opens the
raw data table and fades it out after its first rows. The
README shows the four as a 2×2 grid, so they need about the same
proportions: the heatmap, which spans a whole row of the page, is taken at
a narrower window than the line charts (the width is per chart, in
`SHOTS`). Redo the
images whenever the page's look changes, and keep the README's commands,
and the machine and library versions it names, matching what produced
them.
