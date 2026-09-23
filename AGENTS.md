# AGENTS.md

Guidance for AI coding agents (and humans) working on this repository.

## What this is

BLAS microbenchmarks, structured like [osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/):
one small executable per BLAS routine, sharing a common option/timing/output
layer (`src/c/common`), built with Autotools. See [README.md](README.md) for
the user-facing documentation (install, CLI options, usage examples) — it
covers installing from a release tarball; this file covers working from a
git checkout instead.

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

## Layout

```
src/c/common/    # bmb_options (CLI parsing), bmb_bench (sweep/timing driver),
                 # bmb_result + bmb_print (txt/csv/json), bmb_threads
                 # (thread-count resolution), bmb_build (what this build is),
                 # bmb_log, bmb_timer
src/c/level1/    # dasum, daxpy, dcopy, ddot, dnrm2, dscal, dswap
src/c/level2/    # dgemv, dger, dsymv, dsyr, dsyr2, dtrmv, dtrsv
src/c/level3/    # dgemm, dsymm, dsyrk, dsyr2k, dtrmm, dtrsm
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
   before every call, outside the timing window.
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
`<prog>_SOURCES`) and add a `test_bmb_<routine>.sh` smoke test — copy one
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
  README tells users to run under `numactl` instead. Reasoned from the
  code, not measured: no multi-socket machine has been available.
- **No CPU affinity.** Nothing sets it; the README points at
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

A sixth job, `sanitizers`, rebuilds at `-O1` under ASan and UBSan. It is
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
