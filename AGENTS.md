# AGENTS.md

Guidance for AI coding agents (and humans) working on this repository.

## What this is

BLAS microbenchmarks, structured like [osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/):
one small executable per BLAS routine, sharing a common option/timing/output
layer (`src/c/common`), built with Autotools. The user-facing documentation
is [README.md](README.md) (install and a first run) and the man pages in
`man/` (everything else); see [Documentation](#documentation). This file
covers working from a git checkout.

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
| `src/c/common` | `test_bmb_options` | the CLI parser: sweep forms, ceilings, the point cap, `--label` |
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
| `man` | `test_man_render.sh` | the pages render: placeholders substituted, dated, no groff warning, clean under `mandoc -Tlint`, no command, option or path with a typographic hyphen |
| | `test_man_options.sh` | the pages list exactly the options of `--help`, with the same numeric defaults |
| | `test_man_content.sh` | what the page says against what the programs do: routines and their levels, which dimension is which, the sweep examples, the point limit, the JSON fields (against the keys `bmb_print.c` can write, since a run leaves out those that do not apply — `blas` under Netlib, NVPL and ArmPL), the environment variables, the exit statuses, the commands in the examples |
| | `test_man_html.sh` | the site built from the pages: every page there, every link leads to a file and an anchor of the site or to https, nothing loaded from elsewhere, every section in its page's table of contents, the pages linked to each other, every option of `--help` on its page, the outputs page showing this version's table, CSV and JSON (and the report's screenshots in a git checkout); `html.sh` fails without mandoc or without the benchmarks |
| | `test_man_install.sh` | after `make install`, `man blas-microbenchmark`, `man bmb_report` and `man bmb_<routine>` open, with this build's install path; `make uninstall` leaves nothing |

The two browser tests SKIP without a browser (see [Testing the
page](#testing-the-page)), and the man page tests without groff or
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
  page](#testing-the-page) were learnt. Make sure the mutation still
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
                 # and html.sh, which makes the documentation site of them
doc/             # the README's chart images, and screenshots.sh, which
                 # makes them
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

## Adding a new BLAS routine benchmark

Every routine file follows the same shape (see `src/c/level1/bmb_dasum.c` for
the simplest example, `src/c/level2/bmb_dtrsv.c` for one with an in-place
buffer, `src/c/level3/bmb_dsyrk.c` for a two-real-dimension level 3 routine):

1. A `bmb_ctx_t` struct holding the routine's operands (`malloc`'d in
   `setup()`, freed in `teardown()`).
2. `setup(dim1, dim2, thread_count) -> void *`: allocate and fill inputs.
   `dim2` is `0` when the benchmark's `dim2_label` is `NULL`.
3. `call(void *ctx)`: exactly one call to the `cblas_*` routine and nothing
   else — this is what the timer wraps.
4. `reset(void *ctx)`, optional: restore whatever `call()` consumed. Run
   before every timed batch, outside the timing window. Set
   `reset_every_call` as well if once per batch is not enough — see below.
5. `flops(dim1, dim2)` / `bytes(dim1, dim2)`, optional: the operation count
   and the minimum memory traffic of one call, for the `GFLOP/s` and `GB/s`
   columns. See below for which of the two a routine should declare.
6. `teardown(void *ctx)`: free everything.
7. `main()`: fill a `bmb_benchmark_t` (routine name, dimension labels,
   whether `dim1` sweeps `--vector-size` or `--matrix-dim1`, and the
   function pointers above) and call `bmb_benchmark_main(argc, argv, &bench)`.

A size option is expanded into its explicit list of points while the
command line is parsed (`bmb_range_t` is a list, not a min/max pair), so
`bmb_bench.c` just iterates it and the four sweep forms — single, doubling,
linear step, explicit list — cost the loop nothing. Anything new in that
area belongs in `bmb_parse_range()`, not in the benchmark loop.

Dimension conventions (only two `--matrix-dim*` options exist, so routines
with 3 mathematical dimensions reuse one):

- Vector routines (level 1): `dim1_label = "Vector size"`, `dim2_label = NULL`,
  `use_vector_range = 1`.
- Square matrix routines (`dsymv`, `dsyr`, `dsyr2`, `dtrmv`, `dtrsv`):
  `dim2_label = NULL`, N comes from `dim1` (swept via `--matrix-dim1`).
- Two-real-dimension routines (`dgemv`, `dger`, `dsymm`, `dtrmm`, `dtrsm`):
  `dim1` = M, `dim2` = N.
- `dsyrk`/`dsyr2k`: `dim1` = N, `dim2` = K.
- `dgemm`: `dim1` = M = K (K is tied to M), `dim2` = N.

For every two-dimension routine, `dim2` follows `dim1` point by point unless
`-M` was given, so `-m 512:2048` measures three square problems; `-M` turns
the sweep into the full grid. Until 1.0.0, `-M` silently copied `-m`'s
*range* instead, which made every such sweep the N² grid while `--help`
promised square matrices (#1, decision 3). If you touch `bmb_sweep_dim1()`,
keep the two cases apart.

If a routine overwrites one of its inputs in place (`dtrmv`, `dtrsv`,
`dtrmm`, `dtrsm`) or drifts over repeated calls (`dscal`), keep an
untouched template buffer and `memcpy` it into the working buffer — from
`reset()`, **never from `call()`**. For `dtrmm`/`dtrsm` that copy is
O(M·N), the same order as the routine's own memory traffic, so timing it
would put a large slice of pure `memcpy` into the reported figure.
Routines that merely accumulate into an operand (`daxpy`, `dger`, `dsyr`,
`dsyr2`, ...) don't need a reset: a small `alpha` (e.g. `1.0e-6`) keeps the
accumulation bounded for any iteration count, without a per-call copy.
Never use `alpha == 1.0` for `dscal`: some BLAS implementations
special-case it as a no-op fast path.

Then decide how often that reset has to run. Calls are timed in batches
(see *Known measurement limitations*), and `reset()` runs once per batch,
so the operand drifts across the calls inside one. Ask what that drift
does over a few thousand calls:

- `dtrmv`/`dtrmm` multiply the operand by A every time and `dtrsv`/`dtrsm`
  divide by it, so it reaches infinity or denormals well inside a single
  batch — and denormals are where the hardware slows down and the timing
  stops meaning anything. They set `reset_every_call = 1`, which pins the
  batch to 1 call.
- `dscal` shrinks its vector by `0.999999` per call, which needs about
  10⁸ calls to matter. It leaves the flag at 0 and gets batched.

Setting the flag when it isn't needed is not free: it puts the clock's own
cost back into every measurement, which is the whole point of batching.

### Which rate a routine declares

- `flops`: every routine that does arithmetic. Use the conventional BLAS
  operation counts (`2*M*N*K` for gemm, `K*N*(N+1)` for syrk,
  `N*M*(M+1)` for a left-side trsm, ...) — what the routine is *defined*
  to compute, not what a particular implementation issues. `dcopy` and
  `dswap` declare none.
- `bytes`: levels 1 and 2 only. Count each operand read once and each
  result written once, the way STREAM does — no read-for-ownership — so
  the numbers are comparable with STREAM's. Level 3 must *not* declare it:
  with O(N³) work over O(N²) data the operands live in cache, so a rate
  built from compulsory traffic is not a bandwidth measurement and would
  be read as one.

Sanity-check a new formula by comparing across routines rather than
eyeballing it: at a size large enough to leave the startup noise behind,
every level 3 routine should land on roughly the same GFLOP/s (the
machine's peak) and every level 2 routine on roughly the same GB/s. A
formula off by a factor of two shows up immediately as one routine
beating the rest.

Then wire the new file into the level's `Makefile.am` (`level<N>_PROGRAMS`,
`<prog>_SOURCES`), add the routine to `ROUTINES` in `man/Makefile.am` and
to the NAME line and the routine tables of `man/blas-microbenchmark.1.in`
(`test_man_content.sh` fails until all three are done), and add a
`test_bmb_<routine>.sh` smoke test — copy one
from the same directory, it is a single `bmb_check_run` call naming the
binary, the routine and how many data rows the sweep should produce. Add it
to `TESTS`/`EXTRA_DIST`, `chmod +x` it, then `autoreconf -fi` and rebuild.

**Keep the sizes in those scripts tiny.** They run on every `make check`,
and a benchmark asked for a big size allocates it for real: a test that
once passed `-v 1518500249` put two 12 GB buffers on a 15 GB machine and
froze it, on every single run.

`src/c/test_helper.sh` holds the assertions `bmb_check_run` makes — the
routine header, a timing column, the expected number of data rows, and
every field on those rows a positive number with exactly one nine-decimal
timing column. Exit status alone would accept a benchmark that printed
nothing, or one whose timings had collapsed to zero.

## Backends

| Backend | `configure` flag | Verified |
| --- | --- | --- |
| OpenBLAS | `--with-blas-backend=openblas` (default via `auto`) | CI, `openblas` job |
| BLIS | `--with-blas-backend=blis` | CI, `blis` job |
| Netlib | `--with-blas-backend=netlib` | CI, `netlib` job |
| NVPL | `--with-blas-backend=nvpl` | CI, `nvpl` job (`ubuntu-24.04-arm`) |
| ArmPL | `--with-blas-backend=armpl` | CI, `armpl` job (`ubuntu-24.04-arm`) |
| cuBLAS / rocBLAS | — | Not implemented, not planned |

Every backend is reached through the CBLAS interface, so the same
`bmb_<routine>.c` files link against whichever one `configure` picks; only
`configure.ac`'s detection logic and `bmb_threads.c`'s thread-control
dispatch differ per backend.

Each backend also has to be *nameable*: `configure` resolves the selected
one into `BMB_BLAS_BACKEND` (`auto` becomes whatever `AC_SEARCH_LIBS`
actually linked, since "auto" is not an answer anyone can act on later),
and `BMB_CHECK_BLAS_VERSION_API` probes the call that library exposes its
own version string through. `bmb_build.c` turns both into the provenance
block every result carries. A new backend must set the name; the version
string is a bonus where the library has one (OpenBLAS's
`openblas_get_config()`, BLIS's `bli_info_get_version_str()`) and NULL
where it does not.

The machine side of the provenance lives in `bmb_machine.c`, probed once per
run: CPU model, logical CPUs, NUMA nodes, cpu0's caches, `uname`, and the
date. Three rules there:

- **Every field is best effort and omitted when unreadable** — never
  guessed, never written empty. A container without sysfs cache entries is
  a legitimate "unknown", which is why `test_bmb_machine` checks only what
  every Linux box guarantees plus the sanity of whatever was recorded.
- **On aarch64 there is no `model name`** in `/proc/cpuinfo`, only
  implementer and part codes, so the probe records them raw:
  `implementer 0x41, part 0xd49` on GitHub's arm64 runners (an ARM
  Neoverse-N2). **Never change that form.** The CPU string is part of the
  report's series key, and rewriting it — even into a nicer name — would
  stop files from the same machine merging across versions. Translating
  codes into names is the report's job, for display only (#1, decision 4).
  The `Provenance` step of each CI job prints what every runner reports.
- **No hostname, ever** (#1, decision 4). Reports get shared, and on a
  cluster identical nodes have different names.

`--label` is the escape hatch for what no probe can see. It goes on a
comment line of the text and CSV output, so it refuses control characters:
a newline in it would turn the rest of that line into data rows.

**Netlib** is the exception in how it gets that interface. Reference BLAS
is a Fortran library; some distributions bundle a CBLAS layer in it
(Debian/Ubuntu do — declared by `cblas-netlib.h`, *not* `cblas.h`, which is
why a plain search for `cblas.h` finds nothing and why this file previously
claimed no such layer existed at all), but source builds and cluster
modules often have none. So `src/c/netlib/` provides the `cblas_*` entry
points itself, forwarding to the Fortran symbols, and that directory goes
on the include path ahead of everything else only for this backend, so its
`cblas.h` never shadows a real one. One code path, works either way.

Two things that backend gets wrong easily:

- Plain `-lblas` on Debian/Ubuntu goes through `update-alternatives` and
  normally resolves to *OpenBLAS*, silently benchmarking the wrong library.
  `configure` therefore looks for the reference build's own directory
  (`/usr/lib/*/blas`) first. If you touch that probe, check what
  `ldd src/c/level3/bmb_dgemm` actually resolves to.
- The shim translates row-major CBLAS calls to the column-major Fortran
  ABI. Getting a flip wrong yields a transposed or mirror-triangle result
  silently, with no crash. `src/c/netlib/test_netlib_cblas.c` compares the
  shim against naive reference implementations for exactly this reason, and
  runs as part of `make check` for this backend — every mapping it covers
  has been confirmed to fail when deliberately broken, so treat a failure
  there as real. Add a case for any routine you add to the shim; a mapping
  with no test is a mapping nobody has checked.
- The shim declares the hidden `CHARACTER` length arguments the *GNU*
  Fortran ABI appends. A netlib built with ifort/ifx uses a different
  convention, so that combination is untested — if someone reports it, this
  is where to look.
- A shared `libblas.so` carries its own Fortran runtime; a static
  `libblas.a` does not, and leaves `xerbla`'s `_gfortran_*` symbols
  dangling. `configure` tries the plain link first and retries with
  `-lgfortran`, so don't "simplify" that into a single unconditional
  `-lgfortran` — that would make a Fortran runtime a hard requirement for
  everyone.

**cuBLAS / rocBLAS**: deliberately out of scope. Handle-based,
device-memory API, fundamentally different from the CBLAS interface every
other backend shares — this project tried it once and backed it out; don't
re-add it without being asked.

**NVPL and ArmPL install from real, public, non-interactive apt repos** —
neither needs a login or EULA click-through. If either CI job starts
failing after an upstream version bump, don't assume it's permanently
broken; re-discover the current layout (see below) and fix the probe in
`configure.ac`:

- NVPL's `libnvpl-blas-dev` puts its CBLAS-compatible header at
  `/usr/include/nvpl_compat/cblas.h` (not the default include path) and
  links as `-lnvpl_blas_lp64_gomp`/`-lnvpl_blas_lp64_seq` (+
  `-lnvpl_blas_core`). The installer URL is version-pinned (no stable
  "latest" alias exists) — bump it in the CI job when
  [nvpl-downloads](https://developer.nvidia.com/nvpl-downloads) moves on.
- ArmPL's apt package (`arm-performance-libraries`) installs to a fixed
  `/opt/arm/arm-performance-libraries/{include,lib}` prefix. (The docs
  describe a different, version-numbered `/opt/arm/armpl_<version>_gcc/`
  layout used by the manual tarball installer — that's kept as a fallback
  probe, but the apt package doesn't use it.)
- To re-discover either layout: add a throwaway debug step to the CI job
  (`find /opt/arm`, or `dpkg -L <package> | grep cblas.h`) and read the
  log — don't trust vendor docs over the actual installed files.

### Finding a BLAS that isn't in /usr

Clusters expose BLAS through modules, not distro packages, so `configure`
probes, for the selected backend (plus generic `BLAS_*`/`CBLAS_*`):

- the install prefix, as `<PKG>_ROOT`, `<PKG>_DIR` or `<PKG>_HOME` — three
  names because module files are not consistent about it, and a benchmark
  that cannot find the library it was asked for is no use;
- the directories directly, as `<PKG>_INCDIR`/`<PKG>_INC` and
  `<PKG>_LIBDIR`/`<PKG>_LIB`.

That lives in the `BMB_ENV_HINTS`/`BMB_ENV_PREFIX_HINT`/`BMB_ADD_INCDIR`/
`BMB_ADD_LIBDIR` macros at the top of `configure.ac`.
`--with-blas-incpath`/`--with-blas-libpath` do the same thing explicitly.

Two things to preserve when touching that code:

- Paths are *prepended*, so whatever is added last wins. Backend-specific
  hints are applied after the hardcoded distro probes on purpose, so a
  loaded module beats a system-wide install; and within `BMB_ENV_HINTS`
  the prefixes are applied least-specific first, so `_ROOT` beats `_DIR`
  beats `_HOME`.
- Anything derived from a prefix variable must be guarded on that variable
  being non-empty — which is what `BMB_ENV_PREFIX_HINT` is for.
  `"$FOO_ROOT/lib"` with `FOO_ROOT` unset collapses to `/lib`, which
  exists — and would silently land in `-L`/`-rpath`.

When touching `configure.ac`'s backend-selection logic, don't let checks for
one backend leak into another: e.g. `AC_CHECK_LIB([openblas],
[openblas_set_num_threads])` must only run when OpenBLAS is actually the
library that got linked (see the `auto` case, which branches on
`$ac_cv_search_cblas_dgemm` for exactly this reason) — otherwise a system
that happens to have multiple BLAS libraries installed will `AC_DEFINE` a
`HAVE_*_SET_NUM_THREADS` macro for a library that isn't actually in `LIBS`,
and the link will fail with an undefined reference.

If you add another backend, do it fully — `configure.ac` detection, the
thread-count API in `bmb_threads.c` if it has one, and a CI job — rather
than a partial `--with-blas-backend=X` case that fails at compile or link
time. Leaving a backend entirely unimplemented is fine; leaving it
half-done is not.

## Code style

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

## The report

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
releases (`1.0.0-dev`): its own results have to be accepted by its own
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

### The page

All of it is in `report.html`: plain JavaScript and SVG, no library, in
keeping with "nothing fetched". The decisions it implements are #1's, and
the code points at them; the ones easiest to break by accident:

- **A series is routine + backend + BLAS string + CPU + label.** Files
  sharing one merge; a duplicate point keeps the fastest and is reported.
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

### Testing the page

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

### The README's images

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

## Documentation

Two places, for two readers:

- **README.md** is for the first five minutes: what this is, installing it,
  one run, the report, the backends. Keep it short; it was cut from 458
  lines to 136 once, because nobody found anything in it.
- **The man pages** are the reference: `blas-microbenchmark(1)` for every
  benchmark (options, sweeps, how a number is measured, threads, output,
  exit status, examples) and `bmb_report(1)`. A new option, output field
  or behaviour goes there, not into the README.

`--help` is the summary and ends by pointing at the man page.

**The man pages are tested like code** (`man/test_man_*.sh`, see the
table in [Tests](#tests)), because a page can render perfectly and still
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
  which gives upstream groff's output on any system. The substitution escapes the hyphens of the install path
  itself — with `$(...)`, not backquotes, which eat one level of
  backslashes (they did, once).
- `.nh` and `.ds AD l` at the top turn off hyphenation and justification:
  both look bad in a terminal, and hyphenation splits literals. `.ad l`
  alone does not stick, since the `man` macros reset the adjustment from
  `AD` at every paragraph.
- A page with tables starts with `'\" t`, which tells `man` to run `tbl`.

To read a page without installing it: `man -l man/blas-microbenchmark.1`
from the build directory.

### The site

<https://mathieucaissa.github.io/blas-microbenchmark/> is the man pages
as HTML, and nothing else: one source, checked by the same tests, so the
site cannot say what the pages do not. `make html` builds it in
`man/html/` with mandoc (`man/html.sh`):

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
  has no `doc/`, and the page goes without).
- `outputs.html` shows what the tools produce. The table, CSV and JSON
  are not copies: `html.sh` runs the benchmarks (`BENCH_DIR`, the built
  `src/c`, so `make` comes before `make html`) as it builds the page, and
  shows each command exactly as it ran — they are always this version's
  output, from the machine that built the site. Keep those runs small.
  Below them, the report's summary, four charts and raw data, as
  screenshots from `doc/images`.
- It converts through a file, never a pipe: in a pipe, a mandoc failure
  was hidden behind `sed`'s success and wrote empty pages without a word.
  `test_man_html.sh` checks it fails now.

`.github/workflows/pages.yml` publishes it on every push to `main`, after
running the man page tests strictly on the same commit. The site
therefore describes `main`, which can be ahead of the latest release; the
index says so.

## Known measurement limitations

Recorded here so they are not mistaken for bugs, and not "fixed" without
weighing what the fix costs.

- **The clock is not free.** An empty timed region costs tens of
  nanoseconds. `bmb_bench.c` therefore times a *batch* of calls and divides,
  with the batch size calibrated per data point (two passes: the first
  reading carries the clock's cost and would leave the batch too small).
  `--iterations` counts samples, not calls. Routines that set
  `reset_every_call` are pinned to a batch of 1, so at very small sizes
  their numbers still carry the clock -- that is the price of restoring an
  operand that would otherwise reach infinity or denormals inside a batch.
- **The reported time is the fastest sample.** Not the mean: a batch lasts
  long enough that one scheduling hiccup inflates a sample by a large
  factor, and a handful of samples then drags the mean with it. Measured on
  the development machine, three runs of ddot at n=1024 gave means of 2949,
  396 and 131 ns for fastest samples of 163, 121 and 127 ns. Don't "fix"
  this back to a mean without re-measuring that.
- **First-touch NUMA.** `setup()` allocates and fills operands from the
  main thread, so every page lands on that thread's node. Thread-scaling
  numbers on a multi-socket machine are therefore pessimistic. Making the
  fill loops NUMA-aware means guessing how the BLAS library will
  distribute its own threads, which differs per implementation — so the
  man page tells users to run under `numactl` instead. Reasoned from the
  code, not measured: no multi-socket machine has been available.
- **No CPU affinity.** Nothing sets it; the man page points at
  `OMP_PROC_BIND`/`OMP_PLACES` and `taskset`. Setting affinity from inside
  the benchmark would fight whatever the BLAS library does with its own
  threads.
- **Write failures.** Results are written to stdout as they are measured
  and to `--output` at the end. Both are checked (`ferror`, and `fclose`
  for the file, which is where a buffered write actually reaches the
  disk), and either failure makes the process exit non-zero: a table
  truncated by a full disk must not look like a successful run. `/dev/full`
  is the easy way to test that.

## CI

GitHub Actions (`.github/workflows/ci.yml`) runs `./autogen.sh`,
`configure --enable-werror`, `make`, and `make check` on every push/PR,
once per implemented backend: `openblas`, `blis` and `netlib` on
`ubuntu-latest`, `nvpl` and `armpl` on `ubuntu-24.04-arm` (GitHub's free
Linux arm64 hosted runner for public repos — both those backends are
aarch64-only). Keep it green — a routine that only works "on my machine"
isn't done.

The `openblas` job also installs into a staging root and fails if anything
lands outside `--prefix`, or if `bin/bmb_report` and the `libexec`
benchmarks are not where the README says; and it fails if the browser
tests skipped, since the x86 runners have Chrome.

The `browsers` job builds with OpenBLAS and runs only `src/report`'s tests,
once in Firefox (through geckodriver) and once in Edge, both preinstalled
on the runner image — Edge is installed from Microsoft's repository if it
ever stops being. It exists because Firefox is the browser users are told
to use, and Chrome passing says nothing about it.

The `man` job runs the man page tests in a Fedora container: another
groff and man-db build than Ubuntu's, plus mandoc for its lint, with
`BMB_MAN_STRICT=1` so that a missing tool fails instead of skipping. It
builds without `--enable-werror`, since Fedora's newer GCC is not what it
is there to test; Fedora keeps `cblas.h` in `/usr/include/openblas`, which
it finds through `OPENBLAS_INCDIR`, the hint a cluster module would set.

The site is published by its own workflow, `pages.yml` (see [The
site](#the-site)), not by `ci.yml`.

The `sanitizers` job rebuilds at `-O1` under ASan and UBSan. It is
not a duplicate of the `openblas` job: it sees what a plain build cannot
(out-of-bounds accesses, signed overflow), and the different optimisation
level moves GCC's diagnostics around — the first `-Werror` failure it ever
produced was a format-truncation warning invisible at `-O2`.

Leak detection is on, with the BLAS library's own per-thread buffers --
which it never frees by design -- suppressed by module in
`.github/lsan-suppressions.txt`. That keeps it useful for our code: an
option string that an early return forgot to free is exactly the kind of
thing it catches. To reproduce locally:

```bash
../configure --enable-werror CFLAGS="-O1 -g -fsanitize=address,undefined" \
    LDFLAGS="-fsanitize=address,undefined"
ASAN_OPTIONS=detect_leaks=1 \
  LSAN_OPTIONS=suppressions=$PWD/../.github/lsan-suppressions.txt make check
```

## Cutting a release

End users install from the dist tarball attached to a GitHub release (see
README.md), *not* from GitHub's auto-generated "Source code" archives —
those are plain git exports with no `configure` in them.

`.github/workflows/release.yml` handles that: on a pushed `v*` tag it
builds the tarball, verifies it unpacks and builds with no Autotools
present, then uploads it — *creating* the release if the tag has none yet.

```bash
# 1. bump the version in configure.ac (AC_INIT), commit, push
# 2. tag it -- this is what kicks the workflow off
git tag -a vX.Y.Z -m "..."
git push origin vX.Y.Z
# 3. wait for the workflow (~30s), then write the release notes
gh release edit vX.Y.Z --title vX.Y.Z --notes "..."
```

Note the order: **edit the notes, don't create the release**. The workflow
gets there within about half a minute of the tag push and creates it with
`--generate-notes`, so a `gh release create` afterwards just fails with
"Release.tag_name already exists".

Re-run the upload for an existing tag with
`gh workflow run release.yml -f tag=vX.Y.Z`.
