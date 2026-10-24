# Documentation

The README, the man pages and the site, and how they are tested. Start at
[AGENTS.md](../../AGENTS.md).

Three places, for three readers:

- **README.md** is for the first five minutes: what this is, installing it,
  one run, the report, the backends. Keep it short, and fold what most
  readers skip under `<details>` (#113); it was cut from 458 lines to
  about 140 once, because nobody found anything in it.
- **The man pages** are the reference: `blas-microbenchmark(1)` for every
  benchmark (options, sweeps, how a number is measured, threads, output,
  exit status, examples) and `bmb_report(1)`. A new option, output field
  or behaviour goes there, not into the README.
- **The guides** on the site (`man/site/`, #115) are for a task: install,
  a first run, comparing libraries, the libraries one by one, a cluster,
  ReFrame. They repeat what the man pages and the README say, in another
  order, and are tested against the programs (below).

The site (below) publishes the guides, the man pages, the outputs, and
the tests and CI; all but the guides are generated from their source.

`--help` is the summary and ends by pointing at the man page.

**The man pages are tested like code** (`man/test_man_*.sh`, see the
table in [AGENTS.md, Tests](../../AGENTS.md#tests)), because a page can render perfectly and still
describe last year's program. Most checks run the program and compare
with what the page says, both ways where it can: an option in `--help`
but not in the page fails, and so does a JSON field the page names but no
benchmark writes. So:

- a new option goes into `bmb_options.c` and the page in the same change,
  with the same default;
- a new JSON field, environment variable or exit status goes into the
  page too;
- an example in the page must use a program that exists and options it
  takes; the sweep examples, and the point limit, are run to check the
  sizes they claim.

Each check was made to fail on purpose before it was relied on — a wrong
default, swapped dimensions, a missing field, an alias pointing nowhere,
an uninstall that leaves files — see the commit that added them.

How the man pages are built, and why:

- `man/*.1.in` are the sources. `make` turns them into `*.1`, substituting
  the version and the install path — not `configure`, which would leave a
  literal `${exec_prefix}` in the path. They install to
  `$(mandir)/man1`, so they follow `--prefix` like everything else.
- `make install` also writes one alias page per benchmark
  (`bmb_dgemm.1`: `.so man1/blas-microbenchmark.1`), so that `man
  bmb_dgemm` works; `ROUTINES` in `man/Makefile.am` is the list, which
  `src/c/routines.sh` writes from `src/c/routines.txt`.
- **Every hyphen a reader might paste is written `\-`** — in options,
  paths, commands, `.EX` examples. groff 1.23 renders a plain `-` as a
  typographic hyphen (U+2010) on most systems, which no shell accepts.
  Debian and Ubuntu map it back to ASCII in their `man.local`, so the bug
  is invisible here. `test_man_render.sh` checks the source, and renders
  the pages with an empty `man.local` first in groff's macro path (`-M`),
  which gives upstream groff's output on any system. The substitution
  escapes the hyphens of the install path itself, and of the version (a
  `-dev` one appears in an example; the tests caught it on the first
  `-dev` build) — with `$(...)`, not backquotes, which eat one level of
  backslashes (they did, once).
- Each `/` of the install path is followed by `\:`, an invisible place
  groff may break the line: without it, a long `--prefix` overflows the
  FILES section and `man` prints "cannot break line" warnings above the
  page.
- `.nh` and `.ds AD l` at the top turn off hyphenation and justification:
  both look bad in a terminal, and hyphenation splits literals. `.ad l`
  alone does not stick, since the `man` macros reset the adjustment from
  `AD` at every paragraph.
- A page with tables starts with `'\" t`, which tells `man` to run `tbl`.

To read a page without installing it: `man -l man/blas-microbenchmark.1`
from the build directory.

## The site

<https://mathieucaissa.github.io/blas-microbenchmark/> is built by
`man/html.sh` (`make html`, after `make`, with mandoc, in `man/html/`).
It is a menu of four kinds of page (`MENU` in `html.sh`, which every page
shows on its left; each page has its own table of contents on the right,
and links to the pages before and after it in the menu):

- **The guides**, `man/site/*.html` (#115): Install, Your first
  benchmarks, Compare libraries, The BLAS libraries, On an HPC cluster,
  ReFrame tests. They are written for the site, as fragments of HTML
  whose first lines give the title and description (`<!-- title: … -->`,
  `<!-- description: … -->`), with `<h2 id>` and `<h3 id>` headings and
  `<!-- picture NAME | ALT | CAPTION -->` for a screenshot. Unlike the
  rest of the site, they say again what the man pages and the README say,
  in another order, so `test_man_html.sh` holds them to the programs:
  every command in a `<pre>` runs a program that exists, with options it
  takes (as `test_man_content.sh` does for the man pages' examples). A
  word starting with `bmb_` is taken for a benchmark, so a ReFrame test
  name goes in quotes (`-n 'bmb_run'`). A change to an option, a
  default, a backend or an install step updates them too; they ship in
  the tarball (`SITE_GUIDES` in `man/Makefile.am`). A tarball has no
  `doc/images`, so a guide links to no anchor that only the screenshots
  bring (`outputs.html#report`): `make distcheck` builds the site
  without them, and its link check fails.
- **The man pages**, converted by mandoc (`-Ofragment`), wrapped in the
  site's layout, without what it already says (mandoc's header and
  footer tables, the NAME section). blas-microbenchmark(1) is long, so
  each of its sections goes to a page of its own (`manual_page_of`); a
  new `.SH` stops the build until it has one, and `test_man_html.sh`
  checks that every section is on exactly one page. A link into the long
  page (`blas-microbenchmark.html#ID`), from any page, is pointed at the
  page that holds `ID`; the manual's own references to a section, in
  bold capitals ("see **THREADS**"), become links to its page; a page of
  a single section drops that section's heading, its title already
  saying it. The manual on the site is the installed one, cut up: it
  cannot say what the man page does not.
- `site.css` is the whole stylesheet — mandoc's own is not used. It
  styles the layout, the guides, and the few classes mandoc gives man(7)
  pages (`Sh`, `Ss`, `Pp`, `Bd-indent`, `Bl-tag`, `Bl-bullet`, `tbl`);
  colours are tokens, set for light and for dark. Section names,
  capitals in a man page, are shown in sentence case by CSS
  (`text-transform`); the site's own headings carry `keep-case`. No
  script: on a narrow screen, the menu folds under "Menu" in the bar (a
  `<details>`).
- `html.sh` adds what mandoc leaves undone for man(7) pages: a reference
  such as `bmb_report(1)` becomes a link — to the site's page, or to
  man7.org for the others — and so does a URL. The pages do not use `.UR`
  for URLs: groff then shows only the link text in a terminal, and the
  URL is lost.
- `index.html` is the home page: what it is for, the three steps
  (install, run, compare), the README's ddot chart when `IMAGES` points
  at `doc/images` (`make html` does; a release tarball has no
  `doc/images`, and the page goes without), and a card per guide.
- `outputs.html` shows what the tools produce. The table, CSV and JSON
  are not copies: `html.sh` runs the benchmarks (`BENCH_DIR`, the built
  `src/c`, so `make` comes before `make html`) as it builds the page, and
  shows each command exactly as it ran — they are always this version's
  output, from the machine that built the site. Keep those runs small.
  Below them, the report's summary, four charts and raw data, as
  screenshots from `doc/images`.
- `development.html` lists every test and every CI job (`man/devpage.sh`).
  A test is described by **the first paragraph of its header comment**,
  and a CI job by **the comment above it** in its workflow, its runner,
  its configure line and its step names. Those comments are published:
  write the first paragraph of a new test's comment as a description of
  what it checks, and give a new CI job a comment saying why it exists.
  `test_man_html.sh` fails when a test the Makefiles run, or a job of
  `ci.yml`, is missing from the page.
- It converts through a file, never a pipe: in a pipe, a mandoc failure
  was hidden behind `sed`'s success and wrote empty pages without a word.
  `test_man_html.sh` checks it fails now.

`.github/workflows/pages.yml` publishes it on every push to `main`, after
running the man page tests strictly on the same commit. The site
therefore describes `main`, which can be ahead of the latest release; the
index says so, and the version it shows ends in `-dev` between releases.
