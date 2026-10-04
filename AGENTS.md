# AGENTS.md

Guidance for AI coding agents (and humans) working on this repository.

## What this is

BLAS microbenchmarks, structured like [osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/):
one small executable per BLAS routine, sharing a common option/timing/output
layer (`src/c/common`), built with Autotools. The user-facing documentation
is [README.md](README.md) (install and a first run) and the man pages in
`man/` (everything else); see
[doc/dev/documentation.md](doc/dev/documentation.md). This file covers
working from a git checkout.

It holds what every change needs: building, the branch and pull request,
the tests, the layout, the code style. The rest is in `doc/dev/`, one
file per part of the project; read the one for the part you touch before
changing it:

| If you touch | Read |
|---|---|
| a benchmark, `src/c/common`, timing, rates, `--verify` | [doc/dev/benchmarks.md](doc/dev/benchmarks.md) |
| `configure.ac`, a BLAS library, `bmb_threads.c`, `bmb_build.c`, `bmb_machine.c`, `src/c/netlib` | [doc/dev/backends.md](doc/dev/backends.md) |
| `bmb_report`, `src/report`, `doc/images` | [doc/dev/report.md](doc/dev/report.md) |
| the README, `--help`, `man/`, the site | [doc/dev/documentation.md](doc/dev/documentation.md) |
| `.github/` | [doc/dev/ci.md](doc/dev/ci.md) |
| the version, a release | [doc/dev/release.md](doc/dev/release.md) |

## Build & test

A git checkout has no `configure` yet — `./autogen.sh` (a one-line wrapper
around `autoreconf -fi`) generates it. Release tarballs ship it
pre-generated, so end users skip this step; see README.md.

```bash
sudo apt-get install -y autoconf automake libtool libopenblas-dev pkg-config
sudo apt-get install -y libblis-openmp-dev   # optional, for --with-blas-backend=blis

./autogen.sh
mkdir -p build && cd build   # out-of-tree build keeps the source tree clean
../configure
make
make check       # unit tests + one smoke test per benchmark
```

NVPL and ArmPL (aarch64-only) aren't installable here on x86_64; see their
CI job steps in `.github/workflows/ci.yml` for the exact
`--with-blas-backend=nvpl`/`=armpl` setup.

After changing any `configure.ac` or `Makefile.am`, re-run `./autogen.sh`
before `../configure`/`make` — stale generated `Makefile`s will otherwise
silently ignore your edits.

`make distcheck` (from the build directory) validates the dist tarball and a
clean VPATH build; run it before anything that touches `configure.ac`,
`Makefile.am` files, or adds/removes source files.

## Working on an issue

Every change starts from an issue — open one if there is none — and
reaches `main` through a pull request, never by a direct push:

```bash
git fetch origin
git switch -c issue-<N>-<short-name> origin/main   # e.g. issue-3-cpu-frequency
# ... commits, each one building and passing make check ...
git push -u origin issue-<N>-<short-name>
gh pr create --fill          # the body says "Closes #<N>"
gh pr checks --watch         # every CI job must pass
gh pr merge --merge          # a merge commit keeps the detailed commits
```

- One issue, one branch: work that turns out to be two things becomes two
  issues.
- `main` is protected: a pull request cannot be merged until all nine CI
  jobs pass, and `main` cannot be force-pushed or deleted. The repository
  deletes a branch once its pull request is merged; the issue closes
  itself through "Closes #N".
- Update the branch from `main` (`git merge origin/main`) when `main`
  moved under it, and let CI run again before merging.
- Dependabot's pull requests (GitHub Actions updates) go the same way:
  merge them once CI passes.
- The repository's administrator can still push to `main` directly; that
  is for emergencies, not for work.

## Tests

Everything runs under `make check`, through Automake's test driver: a test
is a program or a `sh` script that exits 0 (pass), 77 (SKIP: something it
needs is missing here) or anything else (fail). Each one leaves its output
in `<name>.log` next to it in the build tree, and the directory's
`test-suite.log` gathers the failures. To run just one, or a few:

```bash
make -C src/report check TESTS=test_bmb_report_js.sh
make -C src/c/common check TESTS='test_bmb_options test_bmb_machine'
```

| Where | Test | What it pins down |
|---|---|---|
| `src/c/common` | `test_bmb_options` | the CLI parser: sweep forms, ceilings, the point cap, `--label`, `--verify` on by default and `-C` |
| | `test_bmb_verify` | what `--verify` rests on: the reference products on hand-worked matrices, the comparison and its message, the tolerance, the probe vector, the corruption hook |
| `src/c` | `test_bmb_verify.sh` | every benchmark passes `--verify` at several sizes, shapes and thread counts, and each one's check catches a corrupted result: exit 2, a message, no results file; a run with no option is checked, one with `-C` is not |
| | `test_bmb_machine` | the machine probe, run over fake `/proc` and `/sys` trees in `fixtures/machine/` — including the aarch64 CPU string that must never change form |
| | `test_netlib_cblas` | (netlib only) the row-major → column-major shim against naive references |
| `src/c/level{1,2,3}` | `test_bmb_<routine>.sh` | each benchmark runs and prints the expected rows (`test_helper.sh`) |
| | `test_bmb_size_limits.sh` | sizes that would wrap `size_t` are refused |
| | `test_bmb_output_formats.sh` | txt/csv/json output, provenance lines, an escaped label |
| | `test_bmb_square_default.sh` | without `-M` a level 2/3 sweep is square; with it, a grid |
| `src/report` | `test_assemble.sh` | `assemble.awk`: byte-for-byte copy, and each refusal |
| | `test_bmb_report.sh` | `bmb_report`: what it refuses, and that the page is self-contained |
| | `test_bmb_report_render.sh` | the page in a real browser: every chart draws |
| | `test_bmb_report_js.sh` | the page's logic, unit by unit (`test_report.js`) |
| top | `test_release_files.sh` | `CHANGELOG.md` and `CITATION.cff` agree with the version: an `[Unreleased]` section for a `-dev` one, a dated section for a release, which `CITATION.cff` cites on the same date |
| | `test_editorconfig.sh` | every text file in git follows `.editorconfig`: UTF-8, LF, a final newline, no trailing whitespace, spaces to indent (a tab first in `Makefile.am`); SKIPs outside a git checkout |
| | `test_actions_pinned.sh` | every `uses:` in the workflows is pinned to a full commit SHA, with its version in a comment; SKIPs without `.github/` (the tarball) |
| | `test_doc_links.sh` | every relative link in the Markdown files leads to a file in git and, for an `#anchor`, to a heading there, as GitHub spells it; SKIPs outside a git checkout |
| `man` | `test_man_render.sh` | the pages render: placeholders substituted, dated, no groff warning, clean under `mandoc -Tlint`, no command, option or path with a typographic hyphen |
| | `test_man_options.sh` | the pages list exactly the options of `--help`, with the same numeric defaults |
| | `test_man_content.sh` | what the page says against what the programs do: routines and their levels, which dimension is which, the sweep examples, the point limit, the JSON fields (against the keys `bmb_print.c` can write, since a run leaves out those that do not apply — `blas` under Netlib, NVPL and ArmPL), the environment variables, the exit statuses, the commands in the examples |
| | `test_man_html.sh` | the site built from the pages: every page there, every link leads to a file and an anchor of the site or to https, nothing loaded from elsewhere, every section in its page's table of contents, the pages linked to each other, every option of `--help` on its page, the outputs page showing this version's table, CSV and JSON (and the report's screenshots in a git checkout); `html.sh` fails without mandoc or without the benchmarks |
| | `test_man_install.sh` | after `make install`, `man blas-microbenchmark`, `man bmb_report` and `man bmb_<routine>` open, with this build's install path; `make uninstall` leaves nothing |

The two browser tests SKIP without a browser (see [Testing the
page](doc/dev/report.md#testing-the-page)), and the man page tests without groff or
`man`; every other test runs anywhere. The mandoc lint is left out, with a
note in the log, where mandoc is not installed. `BMB_MAN_STRICT=1` turns
each of those into a failure — the `man` CI job sets it.

Three rules the existing tests follow, and new ones should too:

- **Tiny sizes only.** A benchmark test that allocates gigabytes takes the
  machine down with it (one did). Test limits through the parser, which
  refuses before allocating.
- **Generate current-format input; keep only old formats as fixtures.**
  Fixtures of the current format fall out of date silently.
- **Prove the test can fail.** Before relying on a new check, break the code
  it guards (a flipped comparison, a removed line) and watch it fail, then
  restore it. Each test here was checked that way; two of them passed when
  broken the first time, which is how the rules in [Testing the
  page](doc/dev/report.md#testing-the-page) were learnt. Make sure the mutation still
  *compiles* — under `--enable-werror` an unused variable stops the build,
  `make check` then runs the old binary, and the test "passes".

## Layout

```
src/c/common/    # bmb_options (CLI parsing), bmb_bench (sweep/timing driver),
                 # bmb_result + bmb_print (txt/csv/json), bmb_threads
                 # (thread-count resolution), bmb_build (what this build is),
                 # bmb_machine (what it runs on), bmb_log, bmb_timer
src/c/level1/    # dasum, daxpy, dcopy, ddot, dnrm2, dscal, dswap
src/c/level2/    # dgemv, dger, dsymv, dsyr, dsyr2, dtrmv, dtrsv
src/c/level3/    # dgemm, dsymm, dsyrk, dsyr2k, dtrmm, dtrsm
src/report/      # bmb_report: the script, the HTML template, how the two
                 # are assembled, fixtures in the formats it refuses, and
                 # the browser tests (browser.sh, test_report.js)
man/             # the man pages, blas-microbenchmark(1) and bmb_report(1),
                 # and html.sh and devpage.sh, which make the documentation
                 # site
doc/             # the README's chart images, and screenshots.sh, which
                 # makes them
doc/dev/         # the rest of this guide, one file per part (see above)
```

`common/` builds into a static convenience library (`libbmbcommon.a`, never
installed); each `level{1,2,3}` routine builds to its own installed
executable named `bmb_<routine>`.

Those go to `$(libexecdir)/blas-microbenchmark/level<N>`, the layout
osu-micro-benchmarks uses, rather than to `bin` — 20 executables named
`bmb_*` have no business sitting in `$PATH`. Each level's `Makefile.am`
declares it with a custom Automake directory variable:

```make
level1dir = $(pkglibexecdir)/level1
level1_PROGRAMS = bmb_dasum ...
```

This is a **deliberate** departure from the GNU standards, which reserve
`libexecdir` for "programs to be run by other programs rather than by
users" and would put these in `bindir`. It was weighed against two
alternatives and kept on purpose, so don't "fix" it:

- *20 binaries flat in `bin`* is what the standards actually call for, and
  what most projects do. It was rejected because the level grouping was
  asked for explicitly.
- *A `bmb` launcher in `bin` running the level binaries from `libexec`*,
  the way `git` uses `libexec/git-core`, would make this layout
  standards-correct. It was rejected as more machinery than the grouping
  is worth. It stays the right answer if the flat `$PATH` ever becomes the
  bigger annoyance.

The cost is that no benchmark lands in `$PATH`; README.md gives the two
lines that put the three directories there. The one exception is
`bmb_report`, a single user-facing command, which goes to `$(bindir)` — and
so follows `--prefix`, `--bindir` and `DESTDIR` like anything else Automake
installs. CI checks that on every push (the `openblas` job's "Install
layout" step: nothing may land outside the prefix), and the release
workflow checks it again from the tarball.

Note it's `level<N>_PROGRAMS`, **not** `check_PROGRAMS` — the latter would
build the benchmarks only under `make check` and install nothing.

## Code style

- `.editorconfig` tells editors the layout: UTF-8, LF line ends, a final
  newline, no trailing whitespace, 4 spaces to indent (2 in HTML, CSS,
  JS, YAML, JSON and Markdown), tabs in `Makefile.am`.
  `test_editorconfig.sh` holds every file in git to it, except the
  width: continuation lines are aligned by hand on what they continue.
  There is no `.clang-format`: the closest configuration still rewrote
  116 lines in 14 files, flattening hand-aligned tables and test
  matrices (issue #15).
- No comments except for non-obvious *why* (a hidden constraint, a numerical
  stability trick, a workaround). Don't restate what the code does.
- Any use of a POSIX function not in strict ISO C (`clock_gettime`,
  `strdup`, ...) needs `#define _POSIX_C_SOURCE 200809L` as the very first
  line of that `.c` file (before any `#include`) — see `bmb_timer.c` /
  `bmb_options.c`. Without it, a strict `-std=c11` build fails.
- Keep `common/` generic: routine-specific logic (shapes, alpha values,
  in-place resets) belongs in the `level{1,2,3}` file, not in `bmb_bench.c`.
- Don't leave an exported function with no callers. `common/` is a
  convenience library for the benchmarks, not a general-purpose one, so an
  unused entry point is dead weight that still has to be read and kept
  compiling.
- The project builds with zero warnings under `-Wall -Wextra`. `configure`
  adds both (after checking the compiler takes them) to `BMB_WARN_CFLAGS`,
  which every `Makefile.am` picks up through `AM_CFLAGS`. They go in
  `AM_CFLAGS` rather than `CFLAGS` so a user-supplied `CFLAGS` still gets
  the last word; `--disable-warnings` turns them off.
- CI adds `--enable-werror` on top, which is what makes that a fact rather
  than an aspiration. It is deliberately *not* the default: a compiler
  newer than a given release will eventually warn about something, and
  that must not stop anyone building it.
- Warnings coming out of a *backend's* headers are the backend's, not
  ours, and must not fail the build: add that directory with `-isystem`
  rather than `-I` (BLIS's `cblas.h` defines static helpers most
  translation units never call, which is two `-Wunused-function` per
  benchmark). Hint directories keep using `-I` and are searched first, so
  a loaded module still wins over a distro probe.
