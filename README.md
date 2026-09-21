# blas-microbenchmark

Microbenchmarks for BLAS routines, structured after the design of
[osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/) (OMB):
one small, self-contained executable per BLAS routine, a common option/timing/
output layer, and an Autotools build.

Unlike OMB (which targets MPI), this project only benchmarks BLAS, so it keeps
a flatter layout: one `src/c` tree with a `common/` helper library and one
directory per BLAS level.

## Status

BLAS levels 1, 2 and 3 (double precision only) are benchmarked against a
choice of backends, selected at `configure` time:

| Backend | `configure` flag | Status |
| --- | --- | --- |
| OpenBLAS | `--with-blas-backend=openblas` (or `auto`) | Verified: built, run, `make check` passes |
| BLIS | `--with-blas-backend=blis` | Verified: built, run, `make check` passes |
| cuBLAS | `--with-gpu-backend=cuda` | Compile/link-verified only — no GPU available to run it in the environment this project was developed in |
| rocBLAS | `--with-gpu-backend=rocm` | Compile/link-verified only — same reason |
| NVPL | `--with-blas-backend=nvpl` | Best-effort: wired per NVPL's published CBLAS-compatible API, but NVPL targets aarch64 (NVIDIA Grace) and could not be built or run here at all |
| ArmPL | `--with-blas-backend=armpl` | Best-effort: wired per ArmPL's published CBLAS-compatible API, but ArmPL requires an Arm-provided install and could not be built or run here at all |
| Netlib | — | Not yet supported: Debian/Ubuntu ships no CBLAS C wrapper for it (only the raw Fortran ABI); see [AGENTS.md](AGENTS.md) |

The CPU backends (OpenBLAS, BLIS, NVPL, ArmPL) all expose the standard
**CBLAS** C interface, so the same `bmb_<routine>` source files link against
whichever one `configure` picks. cuBLAS and rocBLAS have a fundamentally
different, handle-based, device-memory API, so they instead build a second
set of `bmb_<routine>_gpu` executables (only when `--with-gpu-backend` is
given) sharing the same CLI and output format. Running a `*_gpu` benchmark
on a machine with no matching GPU exits with the standard "SKIP" code (77)
rather than failing — `make check` reports it as SKIP, not FAIL.

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
measures (e.g. `bmb_dgemm`, or `bmb_dgemm_gpu` for the GPU build).

## Building

Requires GCC ≥ 12 (this first implementation only targets GCC), Autotools
(autoconf, automake), and one CBLAS-compatible BLAS library:

- [OpenBLAS](https://github.com/OpenMathLib/OpenBLAS) — `libopenblas-dev` on
  Debian/Ubuntu (default backend, no extra flag needed), or
- [BLIS](https://github.com/flame/blis) — `libblis-openmp-dev` on
  Debian/Ubuntu, pass `--with-blas-backend=blis`.

```bash
autoreconf -fi
mkdir -p build && cd build   # out-of-tree build keeps the source tree clean
../configure
make
make check   # runs a minimal smoke test for every benchmark
```

To additionally build the GPU benchmarks, install a GPU toolkit
(`nvidia-cuda-toolkit`, or `librocblas-dev libamdhip64-dev`) and add
`--with-gpu-backend=cuda` or `--with-gpu-backend=rocm` to the `configure`
line above. This builds `bmb_<routine>_gpu` alongside the regular CPU
benchmarks; running one on a machine without the matching GPU prints a
warning and exits 77 (skip) instead of failing.

### Useful `configure` options

| Option | Purpose |
| --- | --- |
| `-h`, `--help` | List all configure options |
| `--prefix=PREFIX` | Installation prefix |
| `--with-blas-backend=auto\|openblas\|blis\|nvpl\|armpl` | CPU BLAS backend to build against (default: `auto`, search for any CBLAS library) |
| `--with-blas-libpath=DIR` | Directory containing the BLAS library to link against, if not on the default path |
| `--with-gpu-backend=none\|cuda\|rocm` | Additionally build the `*_gpu` benchmarks against this GPU backend (default: `none`) |
| `--with-cuda-libpath=DIR` | Directory containing libcublas/libcudart, if not on the default library path |
| `--with-rocm-libpath=DIR` | Directory containing librocblas/libamdhip64, if not on the default library path |
| `CC=...` | C compiler |
| `CFLAGS=...` | C compiler flags |
| `LDFLAGS=...` | Linker flags |
| `LIBS=...` | Extra libraries to link |

Examples:

```bash
# Custom OpenBLAS install
./configure --with-blas-libpath=/opt/openblas/lib CPPFLAGS=-I/opt/openblas/include

# BLIS instead of OpenBLAS
./configure --with-blas-backend=blis

# OpenBLAS (CPU) plus the cuBLAS GPU benchmarks
./configure --with-gpu-backend=cuda
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

4 threads via the BLAS backend's environment variable (`OMP_NUM_THREADS` for
OpenBLAS-OpenMP/NVPL/ArmPL, `BLIS_NUM_THREADS` for BLIS):

```
$ OMP_NUM_THREADS=4 bmb_dgemm
```

If both are set to different values, the command-line option wins and a
warning is printed:

```
$ OMP_NUM_THREADS=4 bmb_dgemm -t 1
OMP_NUM_THREADS is ignored! Set to 4 but option -t is set to 1.
```

GPU benchmarks (`bmb_<routine>_gpu`) don't take `-t`/thread-count env vars —
the GPU backend manages its own parallelism. On a machine with no matching
GPU, they print a warning and exit 77 instead of a result table:

```
$ bmb_dgemm_gpu
No compatible GPU device found; skipping.
```

## License

Apache License 2.0 — see [LICENSE](LICENSE).
