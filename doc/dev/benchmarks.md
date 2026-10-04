# The benchmarks

How a benchmark is written, checked and measured. Start at
[AGENTS.md](../../AGENTS.md).

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
7. `verify(void *ctx, char *msg, size_t size)`: for `--verify`, make one
   call and check its result against a reference — see [Checking
   results](#checking-results---verify).
8. `main()`: fill a `bmb_benchmark_t` (routine name, dimension labels,
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

**`BMB_VERIFY_CORRUPT=1`** makes every `verify()` alter one element of the
library's result by a relative 1e-6 (`bmb_verify_perturb()`) before
comparing. It exists so that `src/c/test_bmb_verify.sh` can prove each of
the 20 checks fails when it should. A new routine's `verify()` must call
`bmb_verify_perturb()` on its result, or that test fails for it. Beyond
the hook, each check was also seen to catch a real wrong computation — a
transposed `dgemm`, a `dsyrk` or `dsyr2` on the wrong triangle, a `dtrsv`
solving with the transpose.

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
