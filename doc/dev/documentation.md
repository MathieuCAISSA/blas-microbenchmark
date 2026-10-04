# Documentation

The README, the man pages and the site, and how they are tested. Start at
[AGENTS.md](../../AGENTS.md).

Two places, for two readers:

- **README.md** is for the first five minutes: what this is, installing it,
  one run, the report, the backends. Keep it short; it was cut from 458
  lines to about 140 once, because nobody found anything in it.
- **The man pages** are the reference: `blas-microbenchmark(1)` for every
  benchmark (options, sweeps, how a number is measured, threads, output,
  exit status, examples) and `bmb_report(1)`. A new option, output field
  or behaviour goes there, not into the README.

The site (below) publishes the man pages, the outputs, and the tests and
CI — each generated from its source, not written twice.

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
  bmb_dgemm` works; `ROUTINES` in `man/Makefile.am` is the list.
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

<https://mathieucaissa.github.io/blas-microbenchmark/> is generated, all
of it: the man pages as HTML, the outputs of real runs, and the tests and
CI as their own sources describe them. Nothing on it is written twice, so
it cannot say what the sources do not. `make html` builds it in
`man/html/` with mandoc (`man/html.sh`), after `make`:

- mandoc converts each page (`-Ofragment`); `html.sh` wraps it in the
  site's layout: a bar to move between pages, a table of contents built
  from the page's sections, the page's name and description as its title,
  a footer. It drops what that layout already says: mandoc's header and
  footer tables, and the NAME section.
- `site.css` is the whole stylesheet — mandoc's own is not used. It
  styles the layout and the few classes mandoc gives man(7) pages (`Sh`,
  `Ss`, `Pp`, `Bd-indent`, `Bl-tag`, `Bl-bullet`, `tbl`); colours are
  tokens, set for light and for dark. Section names, capitals in a man
  page, are shown in sentence case by CSS (`text-transform`), so a section
  named after an acronym would need an exception.
- `html.sh` adds what mandoc leaves undone for man(7) pages: a reference
  such as `bmb_report(1)` becomes a link — to the site's page, or to
  man7.org for the others — and so does a URL. The pages do not use `.UR`
  for URLs: groff then shows only the link text in a terminal, and the
  URL is lost.
- `index.html` is the landing page: the description from the first
  page's NAME line, a card per page, and the README's ddot chart when
  `IMAGES` points at `doc/images` (`make html` does; a release tarball
  has no `doc/images`, and the page goes without).
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
