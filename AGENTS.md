# AGENTS.md

Guidance for AI coding agents (and humans) working on this repository.

## What this is

BLAS microbenchmarks, structured like [osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/):
one small executable per BLAS routine, sharing a common option/timing/output
layer (`src/c/common`), built with Autotools. See [README.md](README.md) for
the user-facing documentation (CLI options, usage examples).

## Build & test

```bash
autoreconf -fi
mkdir -p build && cd build   # out-of-tree build keeps the source tree clean
../configure
make
make check       # runs one smoke test per benchmark (small size, few iterations)
```

Requires GCC >= 12, Autotools (autoconf/automake/libtool), and a BLAS library
exposing `cblas.h` (OpenBLAS's `libopenblas-dev`, or Netlib reference
BLAS+CBLAS). On Debian/Ubuntu:

```bash
sudo apt-get install -y autoconf automake libtool libopenblas-dev pkg-config
```

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
`configure`, `make`, and `make check` against OpenBLAS on every push/PR.
Keep it green — a routine that only works "on my machine" isn't done.

## Backends not yet implemented

`configure` already accepts `--with-cuda-libpath` / `--with-rocm-libpath`
and warns that they're ignored; there is no cuBLAS/rocBLAS/BLIS/NVPL/ArmPL
code yet. Don't half-wire one of these backends — either implement it fully
(thread-count API, build detection, and routine dispatch) or leave the
placeholder as-is.
