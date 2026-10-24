# The benchmarks

How a benchmark is written, checked and measured. Start at
[AGENTS.md](../../AGENTS.md).

## Adding a new BLAS routine benchmark

Every routine file follows the same shape (see
`src/c/level1/double/bmb_dasum.c` for the simplest example,
`src/c/level2/double/bmb_dtrsv.c` for one with an in-place buffer,
`src/c/level3/double/bmb_dsyrk.c` for a two-real-dimension level 3
routine):

1. A `bmb_ctx_t` struct holding the routine's operands (`malloc`'d in
   `setup()`, freed in `teardown()`).
2. `setup(dim1, dim2, thread_count) -> void *`: allocate and fill inputs.
   `dim2` is `0` when the benchmark's `dim2_label` is `NULL`.
3. `call(void *ctx)`: exactly one call to the `cblas_*` routine and nothing
   else — this is what the timer wraps.
4. `reset(void *ctx)`, optional: restore whatever `call()` consumed. Run
   before every timed batch, outside the timing window (after it under
   `--cache cold`). Set `reset_every_call` as well if once per batch is
   not enough — see below.
5. `flops(dim1, dim2)` / `bytes(dim1, dim2)`, optional: the operation count
   and the minimum memory traffic of one call, for the `GFLOP/s` and `GB/s`
   columns. See below for which of the two a routine should declare.
   `footprint(dim1, dim2)`: the bytes of the operands one call touches,
   for `--cache cold`; level 3 declares it, having no `bytes`.

Every operand comes from `bmb_bench_alloc()`, aligned as `--align` says,
never from `malloc` (#163); the context itself, and `verify()`'s scratch,
may. A matrix gets its leading dimension from `bmb_bench_ld(outer,
inner, size)` — `inner` plus `--ld-pad`, 0 when the matrix would not fit
`size_t`, which `setup()` refuses with `bmb_bench_too_large()` before
allocating — and takes `outer * ld` elements. Every place that passes a
leading dimension (the call, `verify()`, `reset()`) uses the stored one,
never a dimension; a linear fill runs over `outer * ld` elements, so that
`--ld-pad 0` gives the values it always did, and an indexed one strides
by the leading dimension, the padding initialised too. `test_bmb_verify.sh`
checks every matrix routine padded, in both storage orders.
6. `teardown(void *ctx)`: free everything.
7. `verify(void *ctx, char *msg, size_t size)`: for `--verify`, make one
   call and check its result against a reference — see [Checking
   results](#checking-results---verify).
8. `main()`: fill a `bmb_benchmark_t` (routine name, dimension labels,
   whether `dim1` sweeps `--vector-size` or `--matrix-dim1`, its `level`,
   whose default size it takes (#165), and the function pointers above)
   and call `bmb_benchmark_main(argc, argv, &bench)`. `test_bmb_levels.sh`
   holds `level` to the routine's directory.

A size option is expanded into its explicit list of points while the
command line is parsed (`bmb_range_t` is a list, not a min/max pair), so
`bmb_bench.c` just iterates it and the four sweep forms — single, doubling,
linear step, explicit list — cost the loop nothing. Anything new in that
area belongs in `bmb_parse_range()`, not in the benchmark loop.

**Storage order (`--layout`, #7).** A routine with a matrix takes its
order from `bmb_bench_column_major()` in `setup()`: it stores the order
to pass CBLAS (`CblasRowMajor` or `CblasColMajor`, kept in an `int`,
since the enum's name differs between libraries) and its leading
dimensions — the number of columns of a matrix row-major, of rows
column-major (`dgemm`: `lda` is K or M, `ldb` N or K, `ldc` N or M). The
problem stays the same: same sizes, same triangle, no transpose; only
its storage changes, and the fills are the same either way. `verify()`
passes the order to `bmb_verify_matvec()`, which reads element (i, j)
where that order puts it. A triangular or symmetric matrix must not be
symmetric in its values off the diagonal, or the upper triangle read one
way equals the other, and `--verify` cannot tell a library that ignored
the order (the triangular routines' `bmb_off_diagonal()`). Each of the
22 routines was seen to fail `--verify` with its call forced to
row-major under `--layout col`.

**Other precisions: families (#6, #67, #166).** The routines of one
operation in several precisions are a family, written once:
`src/c/level3/bmb_gemm.c` is sgemm, dgemm, cgemm and zgemm, compiled once
per precision with `-DBMB_PREC_S`, `_D`, `_C` or `_Z`, which
`routines.sh` writes into the level's `Makefile.am` from the table's
`family` column. `src/c/common/bmb_prec.h` holds what the precision
changes:

- the real type, `bmb_real_t` (`float` for s and c), and the reals per
  element, `BMB_PARTS` (2 for complex, real part first, the layout of
  C99's complex types and of every CBLAS);
- an element's size, `BMB_ELEM_BYTES`, as a number the preprocessor can
  compare: only a routine of 16-byte elements checks its leading
  dimensions for itself (below);
- the CBLAS function and the routine's name, `BMB_CBLAS(gemm)` and
  `BMB_ROUTINE(gemm)`;
- scalars, stored as `BMB_PARTS` reals and passed as CBLAS takes them,
  by value or by address (`BMB_SCALAR`, `bmb_set_scalar`); an array is
  passed as `(const void *)`, which converts to whatever pointer type a
  library's `cblas.h` declares;
- the fill (`bmb_fill`), in the real type, as the one-precision files
  computed it;
- `--verify`'s references, tolerance and perturbation for the precision
  (`bmb_matvec`, `bmb_tolerance`, `bmb_perturb`, `bmb_scale`).

What is the operation's stays in its source, with `#if BMB_COMPLEX` or
`#if BMB_SINGLE` where the precisions differ in substance, as single
precision's scale in GEMM's check. Adding a precision BLAS has to a
family is one line of the table, and its `src/c/netlib/` entry point for
the Netlib build. #200 made GEMM a family, and checked that the four
binaries give exactly what the four files did: the corrupted check's
values to the digit, the counts, the copies `--cache cold` takes, every
field the clock does not change.

The other routines of a precision other than double are still a file
each, the double one's with another element type (`saxpy`, `sdot`,
`sgemv`, `strsm`, `ssyrk`, `ztrsm`), until #201 to #203 make families of
them. `zdotc`, `zherk` and `zhemm` have no real counterpart, and follow
`ddot`, `dsyrk` and `dsymm` with conjugates: `zherk`'s alpha and beta are
real, as BLAS takes them, and `zhemm`'s A has an imaginary part on its
diagonal, which a Hermitian matrix cannot have and BLAS does not read.
Their dimensions and options are the double ones'.

- `flops`: real operations. A complex multiply-add is 4 multiplications
  and 4 additions, so a complex routine counts four times its real
  counterpart (`cgemm`/`zgemm` `8*M*N*K` where `dgemm` counts `2*M*N*K`,
  `zherk` four times `dsyrk`), and their GFLOP/s are comparable.
- `bytes`: 4 per element in single precision, 8 in double.
- Sizes: the parser's ceiling keeps a d x d matrix of 8-byte elements
  within `size_t`, which covers `float`, `double` and single complex.
  A double complex element takes 16, so the z routines' `setup()` check
  their matrices with `bmb_bench_matrix_bytes()` and refuse, with a
  message (`bmb_bench_too_large()`), sizes from 2^30 up;
  `test_bmb_zgemm.sh` runs one.
- `verify()`: the reference stays in double, read from the float
  operands (`bmb_verify_matvec_single()`, `_complex()`,
  `_complex_single()`, every shape), and the result is compared in
  double. Two shapes are the complex ones' own: `FULL_H`, the conjugate
  transpose, and `HERM_UPPER`, a Hermitian matrix from its upper
  triangle, reading neither the lower one nor the diagonal's imaginary
  part.

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
  (and `strsm`/`ztrsm`) divide by it, so it reaches infinity or denormals well inside a single
  batch — and denormals are where the hardware slows down and the timing
  stops meaning anything. They set `reset_every_call = 1`, which pins the
  batch to 1 call, even under an explicit `-b` (ignored with a warning;
  it used to apply, and timed dtrmv 1.5× too fast, #48).
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
- `footprint`: level 3 (#160). The bytes of the operands the call
  touches, not the copy `reset()` restores from. `--cache cold` counts
  its copies from it; without it, from half of `bytes`, a lower bound
  since `bytes` counts an operand at most twice. A routine with neither
  is refused under `--cache cold`.

`src/c/test_bmb_counts.sh` holds every routine of `src/c/routines.txt` to
these conventions, to the unit, at odd sizes and one large one (#205):
its FLOPs, its bytes or footprint, and the absence of what it must not
declare. The formulas there are written from the operation, the
element's size and whether it is complex, not copied from the sources,
so that turning routines into a family (#166) cannot change a count
unseen. A routine of a new operation needs its formula there; the test
fails for it until it has one. It reads the counts with
**`BMB_COUNTS=1`**, a testing aid like `BMB_VERIFY_CORRUPT`: each point's
`flops`, `bytes` and `footprint`, as the routine declares them (`-` for
none), are written to stderr instead of being measured, so nothing is
allocated whatever the size.

Sanity-check a new formula by comparing across routines rather than
eyeballing it: at a size large enough to leave the startup noise behind,
every level 3 routine should land on roughly the same GFLOP/s (the
machine's peak) and every level 2 routine on roughly the same GB/s. A
formula off by a factor of two shows up immediately as one routine
beating the rest.

Then put the file in its level's directory for its precision (`single`,
`double`, `complex` or `complex16`, which is double-precision complex),
with a `test_bmb_<routine>.sh` smoke test beside it: copy one from the
same directory, a single `bmb_check_run` call naming the binary
(`./<precision>/bmb_<routine>`, since tests run from the level's build
directory), the routine and how many data rows the sweep should produce;
`chmod +x` it.

Then add the routine to **`src/c/routines.txt`**, one line: its name,
level, precision, what `-m` and `-M` are (#199), and its family, or `-`
for a source of its own (see below). Every list of the
routines is written from that table, between `BEGIN routines (...)` and
`END routines (...)` lines, by `src/c/routines.sh`, which `./autogen.sh`
runs before `autoreconf`:

- the level's `Makefile.am`: the program in `level<N>_PROGRAMS`
  (`<precision>/bmb_<routine>`), its `_SOURCES` (and, for a family, the
  `_CPPFLAGS` that give its precision), its test in `ROUTINE_TESTS`,
  which `TESTS` and `EXTRA_DIST` follow;
- `ROUTINES` in `man/Makefile.am`, which writes the `man bmb_<routine>`
  pages;
- the NAME line of `man/blas-microbenchmark.1.in`, and its two tables:
  the routines by level, and which dimension `-m` and `-M` are;
- the README's table of the benchmarks;
- the `LEVEL` table of `src/report/report.html`, which orders the
  report's sections and names each routine's level;
- `ROUTINES` in `packaging/reframe/blas_microbenchmark.py`.

Edit the table, never a list: `make check` fails while a list is not
what the table gives (`tests/test_routines.sh`), and the next run of
`routines.sh` would put it back. What is said about the routines in
prose, in the man page and here, is still written by hand. Then
`./autogen.sh` and rebuild.

**Keep the sizes in those scripts tiny.** They run on every `make check`,
and a benchmark asked for a big size allocates it for real: a test that
once passed `-v 1518500249` put two 12 GB buffers on a 15 GB machine and
froze it, on every single run.

`src/c/test_helper.sh` holds the assertions `bmb_check_run` makes — the
routine header, a timing column, the expected number of data rows, and
every field on those rows a positive number with exactly one nine-decimal
timing column. Exit status alone would accept a benchmark that printed
nothing, or one whose timings had collapsed to zero.

### Checking results (`--verify`)

`--verify` is **on by default** (`-C`/`--no-verify` turns it off; `-c`
stays, so that a command line can say it): STREAM and HPL check theirs,
and the check costs next to nothing. It checks each point before timing
it: the driver resets the
operands, calls the routine's `verify()`, and stops the run with exit
status 2 (`BMB_EXIT_WRONG_RESULT`) and no `-o` file if it reports a wrong
result. `verify()` makes one call on the very operands about to be timed
— same size, same thread count, same parameters — and compares the
result with a reference from `bmb_verify.h`:

- levels 1 and 2 are recomputed directly, about one call's work;
- level 3 is checked by random projection (Freivalds): `C*r` against
  `A*(B*r)` for a probe vector `r`, O(N^2) instead of O(N^3); `dtrsm` and
  `dtrsv` by their residual; `dger`, `dsyr`, `dsyr2` by projecting the
  whole matrix, which also covers the triangle they must not touch;
  `dsyrk`/`dsyr2k` project their untouched strictly lower part before and
  after the call, and require it identical to the bit.

Every reference comes with the size of its terms (`|A||x|` for `A*x`), and
the tolerance is a rounding-error bound relative to that:
`bmb_verify_tolerance(terms)` is 2.3e-10 for a 65536-wide product. Do not
widen it to make a failure go away: a check that fails on a correct
library is a wrong reference, to be fixed.

A complex element's size is `|re| + |im|`, which bounds both real
products in each part of a complex product; a complex multiply-add
counts as two terms. A single-precision result uses
`bmb_verify_tolerance_single(terms)`, the same bound with the float
epsilon, 2^29 times wider, counting only the library's terms (the
reference, in double, adds nothing that matters). It is still a bound,
not a guess: a library summing a million floats one after the other,
as Netlib's `sdot` does, drifts by a relative 1e-3 on our data, and a
tighter tolerance would fail it. The price is that a single-precision
check catches a library computing the wrong thing, not one that is
slightly off: `sdot` leaving out one element of a million passes. For
`sgemm` and `cgemm`, the scale of the projection leaves out `|C||r|`
(`dgemm`'s has it): the library's error on `C` is bounded by the
tolerance times `|A||B|`, so on `C*r` by the tolerance times
`|A|(|B||r|)`.

**`BMB_VERIFY_CORRUPT=1`** makes every `verify()` alter one element of the
library's result by a relative 1e-6 (`bmb_verify_perturb()`) before
comparing; in single precision it doubles it, plus one
(`bmb_verify_perturb_single()`), since the projection of a float `cgemm`
does not see even 1e-1 in one element of a 64 x 64 `C`. It exists so
that `src/c/test_bmb_verify.sh` can prove each of
the 32 checks fails when it should. A new routine's `verify()` must call
`bmb_verify_perturb()` or `bmb_verify_perturb_single()` on its result,
or that test fails for it. Beyond
the hook, each check was also seen to catch a real wrong computation — a
transposed `dgemm` or `sgemm`, a `cgemm` or `zgemm` conjugating an
operand, a `dsyrk` or `dsyr2` on the wrong triangle, a `dtrsv` solving
with the transpose, a `saxpy` with twice its alpha.

## Known measurement limitations

Recorded here so they are not mistaken for bugs, and not "fixed" without
weighing what the fix costs.

- **Warm-up and samples by time (#164).** `--warmup-time` (0.2 s by
  default) spins the first point of each thread count, once, rather than
  every point: a sweep of 256 points would otherwise pay it 256 times,
  and the CPU stays at its clock while it is busy. It replaces part of
  the thread start-up a first call paid anyway: `make check` takes no
  longer with it. `--min-time` and `--max-spread` take samples past
  `-i`; the spread is judged each time the samples have doubled
  (sorted in place, O(n log n) in all), on 10 at least, and is the IQR
  over the median, since the coefficient of variation of thousands of
  ddot samples stayed above 100 % on the development machine, a few
  preempted ones keeping the standard deviation up. The buffer for them
  is 100000 samples, set aside whole, so that the allocations a run
  makes do not depend on its timing (`test_bmb_alloc_failures.sh` counts
  them).
- **Hot by default, cold on request (#160).** By default every call
  reuses the operands of the last, so a size that fits in a cache is
  measured from it. `--cache cold` sets up enough copies that the others
  reach twice the last-level cache (`BMB_COLD_REACH`, read from the
  machine block) and rotates over them, one call each; it keeps the
  batch, so the clock is amortised as hot, and a loop of its own, so a
  hot run carries none of the rotation's bookkeeping. `reset()` runs
  right after a copy's call, in the order the copies are used next, so
  that it does not bring them back into the cache. At most 16384 copies
  (`BMB_COLD_MAX_COPIES`): below the size where that falls short, a
  warning says the points are partly warm, and `copies` in the results
  shows by how much. Hardware prefetchers still see each copy's own
  accesses: cold means out of the caches, not unpredictable. Measured on
  the development machine (OpenBLAS, one thread): daxpy at 4096 goes
  from 51 GB/s hot to 11.5 cold, below the 26 GB/s it streams at 16M,
  since a small cold vector waits for memory more than it streams from
  it.

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
  this back to a mean without re-measuring that. The median and the
  quartiles go with it in every output (#161: interpolated as R's type 7,
  the median computed apart so that two samples give their mean to the
  last bit); `-s` adds the mean, the standard deviation and the slowest.
  Variability across whole runs is the report's (#4: repeated runs, a
  band, a Mann-Whitney U test).
- **The run block (#161).** Every output says how it was measured: the
  command line (the program by its basename), `-x`, `-w`, `-i`, `-T`,
  `-e`, `-E`, `-b`, `-k`, `-a` and `-p`, the compiler and the CFLAGS
  configure ended with (`BMB_CFLAGS`, escaped for
  C in `configure.ac`), the affinity as `/proc/self/status` lists it, and
  the variables of `bmb_run_variables` in `bmb_print.c` that are set. A
  variable that changes what a library does goes in that list, and in
  the man page's OUTPUT, which `test_man_content.sh` holds to it.
- **First-touch NUMA.** `setup()` allocates and fills operands from the
  main thread, so every page lands on that thread's node. Thread-scaling
  numbers on a multi-socket machine are therefore pessimistic. Making the
  fill loops NUMA-aware means guessing how the BLAS library will
  distribute its own threads, which differs per implementation — so the
  man page tells users to run under `numactl` instead. Reasoned from the
  code, not measured: no multi-socket machine has been available.
- **No hardware counters (#8).** Reading them is left to `perf stat`
  and `likwid-perfctr`, run around a benchmark; the man page's HARDWARE
  COUNTERS section gives the recipe (two runs differing in `-i`, with
  `-b 1`, so that their difference is the timed calls alone). A built-in
  `--counters` was weighed and left out: WSL and CI runners, being
  virtual, expose no counters, so it could only ever have been tested
  saying "not available".
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
