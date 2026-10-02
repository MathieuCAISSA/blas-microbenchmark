# blas-microbenchmark

[![CI](https://github.com/MathieuCAISSA/blas-microbenchmark/actions/workflows/ci.yml/badge.svg)](https://github.com/MathieuCAISSA/blas-microbenchmark/actions/workflows/ci.yml)
[![License: Apache 2.0](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

Command-line microbenchmarks for BLAS routines — one small executable per
routine, a shared CLI, and plain text/CSV/JSON output. Think
[osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/), but
for BLAS instead of MPI.

Works against whichever BLAS library you already have installed —
OpenBLAS, BLIS, Netlib reference, NVPL or ArmPL — so you can compare them
on the same hardware with the same command, and turn the results into a
page of charts with [`bmb_report`](#charts-bmb_report).

## Install

**Requirements:** a C compiler (GCC ≥ 12) and one BLAS library — at
minimum [OpenBLAS](https://github.com/OpenMathLib/OpenBLAS)
(`libopenblas-dev` on Debian/Ubuntu). BLIS, Netlib reference BLAS, NVPL
and ArmPL also work; see [Backends](#backends).

Take the sources from **`blas-microbenchmark-<version>.tar.gz`** on the
[latest release](https://github.com/MathieuCAISSA/blas-microbenchmark/releases/latest).
It ships `configure` ready to run, so autoconf/automake/libtool are not
needed:

```bash
tar xf blas-microbenchmark-<version>.tar.gz
cd blas-microbenchmark-<version>
./configure
make
make check        # unit tests, plus one smoke test per benchmark
make install      # benchmarks to /usr/local/libexec/blas-microbenchmark,
                  # bmb_report to /usr/local/bin
```

**Working from a git clone?** Then there is one step before that block:
run `./autogen.sh`, which needs autoconf, automake and libtool. A
checkout holds only the files `configure` is generated *from*, never
`configure` itself. The same applies to GitHub's own *"Source code
(zip/tar.gz)"* links on the release page — those are plain git exports,
not the tarball above.

The benchmarks are grouped by BLAS level rather than dumped into `bin`:

```
/usr/local/libexec/blas-microbenchmark/
├── level1/   bmb_dasum  bmb_daxpy  bmb_dcopy  …
├── level2/   bmb_dgemv  bmb_dger   bmb_dsymv  …
└── level3/   bmb_dgemm  bmb_dsymm  bmb_dsyrk  …
```

```
$ /usr/local/libexec/blas-microbenchmark/level1/bmb_daxpy
# blas-microbenchmark 0.6.1
# backend: openblas (OpenBLAS 0.3.26 DYNAMIC_ARCH Haswell MAX_THREADS=64)
# routine: daxpy
Thread count    Vector size     time [s]        GFLOP/s   GB/s
1               4096            0.000001600     5.121     61.446
```

Typing that path every time gets old, so the examples below assume the
three directories are on your `PATH`:

```bash
BMB=/usr/local/libexec/blas-microbenchmark
export PATH="$BMB/level1:$BMB/level2:$BMB/level3:$PATH"
```

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
| `GFLOP/s` | every routine that does arithmetic | the conventional BLAS operation count (`2·M·N·K` for gemm, `K·N·(N+1)` for syrk, …) divided by the reported time |
| `GB/s` | levels 1 and 2 | the bytes the routine must move — each operand read once, each result written once — divided by the reported time |

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
-x, --warmup <n>               untimed calls before measuring (default: 1)
-i, --iterations <n>           timed samples taken (default: 10)
-b, --batch <n>                calls averaged per sample (default: 0, i.e. chosen
                                automatically)
-v, --vector-size <sweep>      vector sizes, level 1 (default: 4096)
-m, --matrix-dim1 <sweep>      matrix first dimension, level 2 & 3 (default: 4096)
-M, --matrix-dim2 <sweep>      matrix second dimension, level 2 & 3
                                (default: square matrices; with -M, every
                                combination of the two)
-t, --thread-count <sweep>     number of BLAS threads (default: 1)
-s, --statistics               add mean/stddev/max and the batch size
-l, --label <text>             tag the results, e.g. "turbo off"
-o, --output <file>            also save results to file
-f, --output-format <fmt>      csv or json (default: csv, or guessed from -o)
-h, --help                     show this help
-V, --version                  show the version, the BLAS backend and the machine
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

For routines with two dimensions, leaving out `-M` measures **square
matrices**, the second dimension following the first at every point. Give
`-M` and every combination is measured instead — the grid that the report's
heatmap is for:

```
$ bmb_dgemm -m 512:2048                  → 512², 1024², 2048²         (3 runs)
$ bmb_dgemm -m 512:2048 -M 512:2048      → every M × N combination  (9 runs)
```

```bash
# sweep vector size, with the measurement detail, saved as CSV
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

## How a number is measured

Reading the clock is not free: an empty timed region costs tens of
nanoseconds, which is the entire duration of a small level 1 call. Timing
each call individually would therefore report mostly the clock.

So each **sample** times a *batch* of calls and divides by the batch size.
`--iterations` is the number of samples (10 by default); the batch size is
chosen automatically per data point so that a batch lasts long enough for
the clock to be irrelevant, and `-s` shows what it settled on. Calls that
already take longer than that get a batch of 1 and are timed exactly as
they always were, so nothing changes for level 3 or for large sizes.
`--batch 1` forces per-call timing back.

What this is worth, on ddot:

| Vector size | per call (`--batch 1`) | batched |
| --- | --- | --- |
| 32 | 95 ns | **24 ns** |
| 128 | 124 ns | **31 ns** |
| 1024 | 286 ns | 237 ns |
| 16384 | 7959 ns | 6350 ns |

**The reported time is the fastest sample, not the mean.** A sample is
already an average over a whole batch, so the fastest one is not a lucky
single call — it is the batch that ran with the least interference from
everything else on the machine. That matters more than it sounds: one
scheduling hiccup inflates a sample enormously, and with ten samples it
drags a mean with it. Three consecutive runs of ddot at n=1024 gave means
of 2949, 396 and 131 ns while their fastest samples were 163, 121 and
127 ns.

`-s` is where variability lives: it adds the mean, the standard deviation
over the samples (population, divided by *n* rather than *n−1*: the
samples are all of what was measured), the slowest sample, and the batch
size.

Results go to stdout, warnings and errors to stderr, so `bmb_dgemm >
results.txt` gets you a clean file.

## Where a result came from

Comparing BLAS libraries is the point, so every result says what produced
it — which library, on which machine, and when:

```
# blas-microbenchmark 0.6.1
# backend: openblas (OpenBLAS 0.3.26 DYNAMIC_ARCH Haswell MAX_THREADS=64)
# cpu: Intel(R) Core(TM) Ultra 7 155U (14 logical CPUs, 1 NUMA node)
# caches: L1d 48K, L1i 64K, L2 2M, L3 12M
# os: Linux 6.6.87 x86_64
# date: 2026-10-02T07:21:37Z
# routine: dgemm
```

The library's own version string is there because two files both saying
`openblas` can be 0.3.20 and 0.3.29, which do not perform alike. The cache
sizes are what lets a reader see where a sweep leaves L1, L2 and L3. A
field that cannot be read on a given machine is left out rather than
guessed.

The **hostname is deliberately not recorded**: results get shared, and the
machine name would travel with them. On a cluster it would also be
misleading, since identical nodes have different names.

What no automatic field can capture — turbo on or off, before and after a
BIOS update, two NUMA bindings, two identical nodes — goes in a label of
your own:

```bash
bmb_dgemm -m 1024:8192 --label "turbo off" -o dgemm-turbo-off.json
```

which adds `# label: turbo off`. Without `--label` there is no label line
at all.

Those same lines head a CSV file, as comments — tell your reader to skip
them (`pd.read_csv("results.csv", comment="#")`). JSON gets real fields
instead:

```json
{
  "version": "0.6.1",
  "backend": "openblas",
  "blas": "OpenBLAS 0.3.26 DYNAMIC_ARCH Haswell MAX_THREADS=64",
  "label": "turbo off",
  "date": "2026-10-02T07:21:37Z",
  "machine": {
    "cpu": "Intel(R) Core(TM) Ultra 7 155U",
    "logical_cpus": 14,
    "numa_nodes": 1,
    "caches": [
      {"level": 1, "type": "data", "size_bytes": 49152},
      {"level": 1, "type": "instruction", "size_bytes": 65536},
      {"level": 2, "type": "unified", "size_bytes": 2097152},
      {"level": 3, "type": "unified", "size_bytes": 12582912}
    ],
    "os": "Linux 6.6.87 x86_64"
  },
  "routine": "dgemm",
  "dim1_label": "Matrix dim1 (M=K)",
  "dim2_label": "Matrix dim2 (N)",
  "results": [ ... ]
}
```

`bmb_<routine> --version` prints the library and the machine without
running anything.

## Charts: `bmb_report`

Save results as JSON, then turn any number of them into one HTML page:

```bash
bmb_dgemm -m 256:4096 -t 1:8 -o results/dgemm-openblas.json
bmb_dgemm -m 256:4096 -t 1:8 -o results/dgemm-blis.json     # built against BLIS
bmb_ddot  -v 1024:16777216  -o results/ddot-openblas.json
bmb_report results/*.json > report.html
firefox report.html
```

The page is **self-contained** — data, styles and script all inside it,
nothing fetched — so it opens straight from disk on a machine with no
network, and can be sent as a single file.

What it shows, each part only when the results can support it:

- **A summary** — the best figure per routine, and where every series came
  from (library, CPU, caches, OS, date).
- **Performance against size**, and **bandwidth** for levels 1 and 2, with
  the points where a sweep leaves L1, L2 and L3 marked on the axis. With
  `-s`, a band shows the spread between the fastest sample and the mean.
- **A comparison** of each library against a reference — OpenBLAS by
  default, switchable at the top of the page — on a log scale, so that
  "twice as slow" and "twice as fast" sit at the same distance from ×1.
- **Thread scaling** at the largest size measured at every thread count,
  against ideal linear scaling.
- **A heatmap of shapes** for two-dimension routines measured as a grid
  (`-m` and `-M` both given).
- **The raw data**, every row, sortable.

Hovering a chart gives the exact values; the arrow keys do the same from
the keyboard.

Results are grouped into **series** by backend, library version, CPU and
`--label`. Files that share all four form one curve — a sweep extended the
next day, or thread counts measured in separate runs, merge on their own.
A point measured in more than one file keeps the fastest measurement, and
the page says so with the gap, so that a run which did not reproduce does
not go unnoticed. To compare two conditions on the same machine — turbo on
and off, two BIOS settings — give each run its own `--label`.

`bmb_report` reads results from the same major version as itself, and
refuses anything else **before writing a page**, naming the file and the
reason: results from before 0.6.0 measured a mean rather than the fastest
sample, and 0.6.x did not record the machine, so neither can be compared
with current results.

## Getting numbers you can trust

Two things the benchmarks do not manage for you, and both matter as soon
as you use more than one thread.

**Pin the threads.** Nothing here sets affinity, so the scheduler is free
to move threads between cores mid-measurement. For the OpenMP-threaded
backends (BLIS, NVPL, ArmPL, and OpenBLAS built with OpenMP):

```bash
OMP_PROC_BIND=close OMP_PLACES=cores bmb_dgemm -t 1:8
```

A pthread-built OpenBLAS ignores those, so pin the process instead:
`taskset -c 0-7 bmb_dgemm -t 8`.

**Watch out for NUMA.** Operands are allocated and filled by the main
thread before the BLAS call, so Linux's first-touch policy puts every page
on that thread's node. On a single-socket machine this changes nothing. On
a multi-socket one, threads on the other sockets reach their data
remotely, and thread-scaling figures come out pessimistic — the benchmark
measures the interconnect as much as the routine. Either spread the pages:

```bash
numactl --interleave=all bmb_dgemm -t 1:32
```

or measure one socket at a time, which is usually what you actually want
to compare:

```bash
numactl --cpunodebind=0 --membind=0 bmb_dgemm -t 1:16
```

This is a known limitation rather than a bug: making the fill loops
NUMA-aware would mean guessing how the BLAS library will distribute its
own threads, which differs between implementations.

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
