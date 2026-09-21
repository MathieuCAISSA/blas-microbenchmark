# blas-microbenchmark

Microbenchmarks for BLAS routines, structured after the design of
[osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/) (OMB):
one small, self-contained executable per BLAS routine, a common option/timing/
output layer, and an Autotools build.

Unlike OMB (which targets MPI), this project only benchmarks BLAS, so it keeps
a flatter layout: one `src/c` tree with a `common/` helper library and one
directory per BLAS level.

## Status

First implementation: BLAS levels 1, 2 and 3 (double precision only) are
benchmarked against any BLAS library that exposes the standard **CBLAS**
C interface — Netlib reference BLAS/CBLAS, or OpenBLAS.

cuBLAS, rocBLAS, BLIS, NVPL and ArmPL are on the roadmap; `configure` already
accepts `--with-cuda-libpath` and `--with-rocm-libpath` so the interface is
stable, but those backends are not wired into the build yet.

## Layout

```
blas-microbenchmark/
├── configure.ac
├── Makefile.am
├── LICENSE
├── README.md
└── src
    └── c
        ├── common          # options, logging, timing, results, printing, threading
        ├── level1          # dasum, daxpy, dcopy, ddot, dnrm2, dscal, dswap
        ├── level2          # dgemv, dger, dsymv, dsyr, dsyr2, dtrmv, dtrsv
        └── level3          # dgemm, dsymm, dsyrk, dsyr2k, dtrmm, dtrsm
```

Each benchmark builds to its own executable, named after the routine it
measures (e.g. `bmb_dgemm`).

## Building

Requires GCC ≥ 12 (this first implementation only targets GCC), Autotools
(autoconf, automake), and a BLAS library exposing `cblas.h` — either:

- Netlib reference BLAS + CBLAS (`libblas`, `libcblas`, headers), or
- [OpenBLAS](https://github.com/OpenMathLib/OpenBLAS) (`libopenblas-dev` on
  Debian/Ubuntu already ships `cblas.h`).

```bash
autoreconf -fi
./configure
make
make check   # runs a minimal smoke test for every benchmark
```

### Useful `configure` options

| Option | Purpose |
| --- | --- |
| `-h`, `--help` | List all configure options |
| `--prefix=PREFIX` | Installation prefix |
| `--with-blas-libpath=DIR` | Directory containing the BLAS library to link against |
| `--with-cuda-libpath=DIR` | Reserved for the future cuBLAS backend |
| `--with-rocm-libpath=DIR` | Reserved for the future rocBLAS backend |
| `CC=...` | C compiler |
| `CFLAGS=...` | C compiler flags |
| `LDFLAGS=...` | Linker flags |
| `LIBS=...` | Extra libraries to link |

Example, pointing at a custom OpenBLAS install:

```bash
./configure --with-blas-libpath=/opt/openblas/lib CPPFLAGS=-I/opt/openblas/include
```

## Running a benchmark

Every benchmark shares the same command-line interface:

```
Usage: bmb_<routine> [OPTIONS]

  -x, --warmup <n>               iterations ignored before timing (default: 1)
  -i, --iterations <n>           iterations measured (default: 10)
  -v, --vector-size <[min:]max>  vector size range, level 1 & 2 (default: 4096)
  -m, --matrix-dim1 <[min:]max>  matrix first-dimension range, level 2 & 3 (default: 4096)
  -M, --matrix-dim2 <[min:]max>  matrix second-dimension range, level 2 & 3
                                  (default: same as --matrix-dim1, i.e. square matrices)
  -t, --thread-count <[min:]max> number of BLAS threads (default: 1)
  -s, --statistics               add stddev/min/max columns (default: off)
  -o, --output <filename>        also save results to filename
  -f, --output-format <fmt>      csv or json (default: csv, or inferred from -o's extension)
  -h, --help                     show this help
```

A `[min:]max` range is swept by doubling from `min` to `max` (e.g. `256:4096`
sweeps 256, 512, 1024, 2048, 4096). A bare `max` runs that single size.

Level 1 routines sweep `--vector-size`. Level 2 and 3 routines sweep
`--matrix-dim1`/`--matrix-dim2`; routines that only have one meaningful
dimension (e.g. `dsymv`, a symmetric N×N matrix) ignore `--matrix-dim2` and
only sweep `--matrix-dim1`. `dgemm` reuses `--matrix-dim1` for both its M and
K dimensions, since only two size options are exposed by design.

### Examples

Default run (1 thread, 4096-element vectors):

```
$ bmb_daxpy
# routine: daxpy
Thread count    Vector size     time [s]
1               4096            0.000003
```

4 threads via the command line:

```
$ bmb_dgemm -t 4
```

4 threads via the BLAS backend's environment variable:

```
$ OMP_NUM_THREADS=4 bmb_dgemm
```

If both are set to different values, the command-line option wins and a
warning is printed:

```
$ OMP_NUM_THREADS=4 bmb_dgemm -t 1
OMP_NUM_THREADS is ignored! Set to 4 but option -t is set to 1.
```

## License

Apache License 2.0 — see [LICENSE](LICENSE).
