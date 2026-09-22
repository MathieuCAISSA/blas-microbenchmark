# blas-microbenchmark

[![CI](https://github.com/MathieuCAISSA/blas-microbenchmark/actions/workflows/ci.yml/badge.svg)](https://github.com/MathieuCAISSA/blas-microbenchmark/actions/workflows/ci.yml)
[![License: Apache 2.0](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

Command-line microbenchmarks for BLAS routines — one small executable per
routine, a shared CLI, and plain text/CSV/JSON output. Think
[osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/), but
for BLAS instead of MPI.

Works against whichever BLAS library you already have installed —
OpenBLAS, BLIS, Netlib reference, NVPL or ArmPL — so you can compare them
on the same hardware with the same command.

## Install

**Requirements:** a C compiler (GCC ≥ 12) and one BLAS library — at
minimum [OpenBLAS](https://github.com/OpenMathLib/OpenBLAS)
(`libopenblas-dev` on Debian/Ubuntu). BLIS, Netlib reference BLAS, NVPL
and ArmPL also work; see [Backends](#backends).

```bash
./configure
make
make check        # sanity-checks every benchmark
make install      # installs to /usr/local/libexec/blas-microbenchmark
```

The benchmarks are grouped by BLAS level rather than dumped into `bin`:

```
/usr/local/libexec/blas-microbenchmark/
├── level1/   bmb_dasum  bmb_daxpy  bmb_dcopy  …
├── level2/   bmb_dgemv  bmb_dger   bmb_dsymv  …
└── level3/   bmb_dgemm  bmb_dsymm  bmb_dsyrk  …
```

```
$ /usr/local/libexec/blas-microbenchmark/level1/bmb_daxpy
# routine: daxpy
Thread count    Vector size     time [s]        GFLOP/s   GB/s
1               4096            0.000001404     5.834     70.002
```

Typing that path every time gets old, so the examples below assume the
three directories are on your `PATH`:

```bash
BMB=/usr/local/libexec/blas-microbenchmark
export PATH="$BMB/level1:$BMB/level2:$BMB/level3:$PATH"
```

Take the sources from **`blas-microbenchmark-<version>.tar.gz`** on the
[latest release](https://github.com/MathieuCAISSA/blas-microbenchmark/releases/latest):
it ships `configure` pre-generated, so no autoconf/automake/libtool
needed. (GitHub's own *"Source code (zip/tar.gz)"* links on the same page
are plain git exports **without** `configure` — if you took one of those,
or cloned the repo, run `./autogen.sh` first.)

Installing somewhere other than `/usr/local`? `./configure --prefix=DIR`,
e.g. `"$HOME/.local"`.

**On a cluster**, where BLAS usually comes from a module rather than
`/usr`, `configure` picks up the usual environment variables by itself —
no need to spell out include/library paths:

```bash
module load openblas     # sets OPENBLAS_ROOT / OPENBLAS_INCDIR / OPENBLAS_LIBDIR
./configure              # finds cblas.h and libopenblas through them
```

`<PKG>_ROOT`/`<PKG>_DIR`/`<PKG>_HOME`, `<PKG>_INCDIR` and `<PKG>_LIBDIR`
are honoured for `OPENBLAS`,
`BLIS`, `NETLIB`, `NVPL`, `ARMPL`, plus generic `BLAS_*`/`CBLAS_*`. Anything unusual
can still be pointed at explicitly with `--with-blas-incpath=DIR` and
`--with-blas-libpath=DIR`.

## The benchmarks

20 double-precision routines across the three BLAS levels, each its own
`bmb_<routine>` executable:

| Level | Routines |
| --- | --- |
| 1 (vector-vector) | `dasum` `daxpy` `dcopy` `ddot` `dnrm2` `dscal` `dswap` |
| 2 (matrix-vector) | `dgemv` `dger` `dsymv` `dsyr` `dsyr2` `dtrmv` `dtrsv` |
| 3 (matrix-matrix) | `dgemm` `dsymm` `dsyrk` `dsyr2k` `dtrmm` `dtrsm` |

## What gets reported

Alongside the wall-clock time, each benchmark reports the rate that
actually tells you something about the routine:

| Column | Shown for | Meaning |
| --- | --- | --- |
| `GFLOP/s` | every routine that does arithmetic | the conventional BLAS operation count (`2·M·N·K` for gemm, `K·N·(N+1)` for syrk, …) divided by the mean time |
| `GB/s` | levels 1 and 2 | the bytes the routine must move — each operand read once, each result written once — divided by the mean time |

`dcopy` and `dswap` perform no arithmetic, so they get no `GFLOP/s`
column. Level 3 routines get no `GB/s` one: they reuse their operands out
of cache (O(N³) work over O(N²) data), so a rate built from compulsory
traffic would invite a comparison with STREAM that means nothing. Levels 1
and 2 stream their operands once, and *are* bandwidth-bound, which is
exactly what that column is for — counted the way STREAM counts it, so the
two are comparable.

## Options

Every benchmark takes the same flags:

```
-x, --warmup <n>               iterations ignored before timing (default: 1)
-i, --iterations <n>           iterations measured (default: 10)
-v, --vector-size <sweep>      vector sizes, level 1 (default: 4096)
-m, --matrix-dim1 <sweep>      matrix first dimension, level 2 & 3 (default: 4096)
-M, --matrix-dim2 <sweep>      matrix second dimension, level 2 & 3
                                (default: same as --matrix-dim1, i.e. square)
-t, --thread-count <sweep>     number of BLAS threads (default: 1)
-s, --statistics               add stddev/min/max columns
-o, --output <file>            also save results to file
-f, --output-format <fmt>      csv or json (default: csv, or guessed from -o)
-h, --help                     show this help
```

The four size options take a `<sweep>`, which says which sizes to measure:

| Form | Measures | Example |
| --- | --- | --- |
| `max` | that one size | `4096` |
| `min:max` | doubling | `256:4096` → 256, 512, 1024, 2048, 4096 |
| `min:max:step` | linear steps | `1000:4000:1000` → 1000, 2000, 3000, 4000 |
| `v1,v2,...` | exactly those sizes | `64,1000,4096` |

`max` is always measured, even when the stride would overshoot it
(`100:1000:300` → 100, 400, 700, **1000**). A sweep is capped at 256
points.

```bash
# sweep vector size, with stddev/min/max, saved as CSV
bmb_ddot -v 1024:16384 -s -o results.csv

# the sizes that matter to you, and nothing else
bmb_dgemm -m 1024,2048,4096

# 4 threads, either way works the same:
bmb_dgemm -t 4
OMP_NUM_THREADS=4 bmb_dgemm
```

If the thread-count flag and a thread-count environment variable the
backend reads disagree, the flag wins and a warning explains why:

```
$ OMP_NUM_THREADS=4 bmb_dgemm -t 1
OMP_NUM_THREADS is ignored! Set to 4 but option -t is set to 1.
```

Which variables those are depends on the backend, and they are checked in
the order the backend itself would: `OPENBLAS_NUM_THREADS`,
`GOTO_NUM_THREADS` then `OMP_NUM_THREADS` for OpenBLAS; `BLIS_NUM_THREADS`
then `OMP_NUM_THREADS` for BLIS; `OMP_NUM_THREADS` for NVPL and ArmPL.
Netlib reference BLAS is single-threaded and has no thread-count API at
all, so `-t` has no effect there.

Results go to stdout, warnings and errors to stderr, so `bmb_dgemm >
results.txt` gets you a clean file.

## Backends

Pick one at `configure` time — no code changes needed, and every one of
them is built and tested in CI:

| Backend | `configure` flag |
| --- | --- |
| OpenBLAS *(default)* | *(none needed, or `--with-blas-backend=openblas`)* |
| BLIS | `--with-blas-backend=blis` |
| Netlib reference | `--with-blas-backend=netlib` |
| NVPL *(aarch64)* | `--with-blas-backend=nvpl` |
| ArmPL *(aarch64)* | `--with-blas-backend=armpl` |

cuBLAS/rocBLAS aren't planned. See [AGENTS.md](AGENTS.md) for each
backend's install steps and internals.

`./configure --help` lists every option, including `--with-blas-libpath`
for a non-standard install location and the usual `CC`/`CFLAGS`/`LDFLAGS`.

## License

[Apache License 2.0](LICENSE).
