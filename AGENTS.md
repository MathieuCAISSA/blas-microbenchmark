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

A git checkout has no `configure` yet — `autoreconf` generates it (a
release tarball ships it pre-generated, so end users skip this step; see
README.md).

```bash
sudo apt-get install -y autoconf automake libtool libopenblas-dev pkg-config
sudo apt-get install -y libblis-openmp-dev   # optional, for --with-blas-backend=blis

autoreconf -fi
mkdir -p build && cd build   # out-of-tree build keeps the source tree clean
../configure
make
make check       # runs one smoke test per benchmark (small size, few iterations)
```

NVPL and ArmPL (aarch64-only) aren't installable here on x86_64; see their
CI job steps in `.github/workflows/ci.yml` for the exact
`--with-blas-backend=nvpl`/`=armpl` setup.

After changing any `configure.ac` or `Makefile.am`, re-run `autoreconf -fi`
before `../configure`/`make` — stale generated `Makefile`s will otherwise
silently ignore your edits.

`make distcheck` (from the build directory) validates the dist tarball and a
clean VPATH build; run it before anything that touches `configure.ac`,
`Makefile.am` files, or adds/removes source files.

## Layout

```
src/c/common/    # bmb_options (CLI parsing), bmb_bench (sweep/timing driver),
                 # bmb_result + bmb_print (txt/csv/json), bmb_threads
                 # (thread-count resolution), bmb_log, bmb_timer
src/c/level1/    # dasum, daxpy, dcopy, ddot, dnrm2, dscal, dswap
src/c/level2/    # dgemv, dger, dsymv, dsyr, dsyr2, dtrmv, dtrsv
src/c/level3/    # dgemm, dsymm, dsyrk, dsyr2k, dtrmm, dtrsm
```

`common/` builds into a static convenience library (`libbmbcommon.a`, never
installed); each `level{1,2,3}` routine builds to its own installed
executable named `bmb_<routine>` (`bin_PROGRAMS` — not `check_PROGRAMS`, so
`make`/`make install` produce real binaries, not just test-only ones).

## Adding a new BLAS routine benchmark

Every routine file follows the same shape (see `src/c/level1/bmb_dasum.c` for
the simplest example, `src/c/level2/bmb_dtrsv.c` for one with an in-place
buffer, `src/c/level3/bmb_dsyrk.c` for a two-real-dimension level 3 routine):

1. A `bmb_ctx_t` struct holding the routine's operands (`malloc`'d in
   `setup()`, freed in `teardown()`).
2. `setup(dim1, dim2, thread_count) -> void *`: allocate and fill inputs.
   `dim2` is `0` when the benchmark's `dim2_label` is `NULL`.
3. `call(void *ctx)`: exactly one call to the `cblas_*` routine — this is the
   timed portion, keep it to just the call (plus any required reset, see
   below).
4. `teardown(void *ctx)`: free everything.
5. `main()`: fill a `bmb_benchmark_t` (routine name, dimension labels,
   whether `dim1` sweeps `--vector-size` or `--matrix-dim1`, and the three
   function pointers above) and call `bmb_benchmark_main(argc, argv, &bench)`.

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
`dtrmm`, `dtrsm`, ...), keep an untouched template buffer and `memcpy` it
into the working buffer at the start of `call()`, so results stay correct
and stable regardless of `--iterations`. Routines that accumulate into an
operand across calls (`daxpy`, `dger`, `dsyr`, `dsyr2`, ...) instead use a
small `alpha` (e.g. `1.0e-6`) to keep the accumulation bounded for any
iteration count. Never use `alpha == 1.0` for `dscal`: some BLAS
implementations special-case it as a no-op fast path.

Then wire the new file into the level's `Makefile.am` (`bin_PROGRAMS`,
`<prog>_SOURCES`) and add a `test_bmb_<routine>.sh` smoke-test script (copy
an existing one in the same directory, adjust the binary name — small size,
`-x 1 -i 2`), add it to `TESTS`/`EXTRA_DIST`, `chmod +x` it, then
`autoreconf -fi` and rebuild.

## Backends

| Backend | `configure` flag | Verified |
| --- | --- | --- |
| OpenBLAS | `--with-blas-backend=openblas` (default via `auto`) | CI, `openblas` job |
| BLIS | `--with-blas-backend=blis` | CI, `blis` job |
| NVPL | `--with-blas-backend=nvpl` | CI, `nvpl` job (`ubuntu-24.04-arm`) |
| ArmPL | `--with-blas-backend=armpl` | CI, `armpl` job (`ubuntu-24.04-arm`) |
| Netlib | — | Not implemented — no code |
| cuBLAS / rocBLAS | — | Not implemented, not planned |

All four implemented backends expose the standard CBLAS interface, so the
same `bmb_<routine>.c` files link against whichever one `configure` picks;
only `configure.ac`'s backend-detection logic and `bmb_threads.c`'s
thread-control dispatch differ per backend.

**Netlib**: Debian/Ubuntu's `libblas-dev` has no CBLAS C wrapper, only the
raw Fortran ABI (no `cblas.h`, no `cblas_dgemm` symbol). Supporting it
needs a small hand-written CBLAS-over-Fortran shim — nothing in the tree
provides one yet.

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

When touching `configure.ac`'s backend-selection logic, don't let checks for
one backend leak into another: e.g. `AC_CHECK_LIB([openblas],
[openblas_set_num_threads])` must only run when OpenBLAS is actually the
library that got linked (see the `auto` case, which branches on
`$ac_cv_search_cblas_dgemm` for exactly this reason) — otherwise a system
that happens to have multiple BLAS libraries installed will `AC_DEFINE` a
`HAVE_*_SET_NUM_THREADS` macro for a library that isn't actually in `LIBS`,
and the link will fail with an undefined reference.

If you're implementing Netlib, do it fully — the CBLAS-over-Fortran shim,
`configure.ac` detection, and a CI job for it — rather than adding a
partial `--with-blas-backend=netlib` case that fails at compile or link
time. Leaving a backend entirely unimplemented (as today, for Netlib) is
fine; leaving it half-done is not.

## Code style

- No comments except for non-obvious *why* (a hidden constraint, a numerical
  stability trick, a workaround). Don't restate what the code does.
- Any use of a POSIX function not in strict ISO C (`clock_gettime`,
  `strdup`, ...) needs `#define _POSIX_C_SOURCE 200809L` as the very first
  line of that `.c` file (before any `#include`) — see `bmb_timer.c` /
  `bmb_options.c`. Without it, a strict `-std=c11` build fails.
- Keep `common/` generic: routine-specific logic (shapes, alpha values,
  in-place resets) belongs in the `level{1,2,3}` file, not in `bmb_bench.c`.
- The project should build with zero warnings under `-Wall -Wextra`.

## CI

GitHub Actions (`.github/workflows/ci.yml`) runs `autoreconf -fi`,
`configure`, `make`, and `make check` on every push/PR, once per
implemented backend: `openblas` and `blis` on `ubuntu-latest`, `nvpl` and
`armpl` on `ubuntu-24.04-arm` (GitHub's free Linux arm64 hosted runner for
public repos — both those backends are aarch64-only). Keep it green — a
routine that only works "on my machine" isn't done.

## Cutting a release

End users install from a GitHub release tarball (see README.md), not a git
checkout, so a release needs the dist tarball built and attached, not just
a tag:

```bash
# bump the version in configure.ac (AC_INIT) first if needed
autoreconf -fi
mkdir -p build && cd build && ../configure && make dist
cd ..
git tag -a vX.Y.Z -m "..."
git push origin vX.Y.Z
gh release create vX.Y.Z build/blas-microbenchmark-X.Y.Z.tar.gz --title vX.Y.Z --notes "..."
```

Before tagging, verify the tarball actually works standalone — extract it
somewhere *outside* the repo and run `./configure && make && make check &&
make install` with **no** `autoreconf` step, confirming it needs no
Autotools installed. Then update the download URL in README.md's Install
section to match the new tag.
