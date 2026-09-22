# blas-microbenchmark

[![CI](https://github.com/MathieuCAISSA/blas-microbenchmark/actions/workflows/ci.yml/badge.svg)](https://github.com/MathieuCAISSA/blas-microbenchmark/actions/workflows/ci.yml)
[![License: Apache 2.0](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

Command-line microbenchmarks for BLAS routines — one small executable per
routine, a shared CLI, and plain text/CSV/JSON output. Think
[osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/), but
for BLAS instead of MPI.

Works against whichever BLAS library you already have installed —
OpenBLAS, BLIS, NVPL, or ArmPL — so you can compare them on the same
hardware with the same command.

## Install

Grab the [latest release](https://github.com/MathieuCAISSA/blas-microbenchmark/releases/latest)
and build it the classic Autotools way — `configure` ships pre-generated,
so you don't need autoconf/automake/libtool, just a BLAS library and a
C compiler:

```bash
sudo apt-get install -y libopenblas-dev   # or another backend, see below

curl -LO https://github.com/MathieuCAISSA/blas-microbenchmark/releases/download/v0.1.0/blas-microbenchmark-0.1.0.tar.gz
tar xf blas-microbenchmark-0.1.0.tar.gz
cd blas-microbenchmark-0.1.0

./configure
make
make check        # sanity-checks every benchmark
sudo make install # installs bmb_<routine> to /usr/local/bin
```

Then run one:

```
$ bmb_daxpy
# routine: daxpy
Thread count    Vector size     time [s]
1               4096            0.000003
```

Prefer not to use `sudo`? `./configure --prefix="$HOME/.local"` installs
there instead — just make sure `$HOME/.local/bin` is on your `PATH`.

Working from a git checkout instead of a release tarball? Same steps, plus
generating `configure` yourself first:

```bash
sudo apt-get install -y autoconf automake libtool libopenblas-dev pkg-config
git clone https://github.com/MathieuCAISSA/blas-microbenchmark.git
cd blas-microbenchmark
autoreconf -fi
./configure && make && make check && sudo make install
```

## The benchmarks

20 double-precision routines across the three BLAS levels, each its own
`bmb_<routine>` executable after `make`:

| Level | Routines |
| --- | --- |
| 1 (vector-vector) | `dasum` `daxpy` `dcopy` `ddot` `dnrm2` `dscal` `dswap` |
| 2 (matrix-vector) | `dgemv` `dger` `dsymv` `dsyr` `dsyr2` `dtrmv` `dtrsv` |
| 3 (matrix-matrix) | `dgemm` `dsymm` `dsyrk` `dsyr2k` `dtrmm` `dtrsm` |

## Options

Every benchmark takes the same flags:

```
-x, --warmup <n>               iterations ignored before timing (default: 1)
-i, --iterations <n>           iterations measured (default: 10)
-v, --vector-size <[min:]max>  vector size range, level 1 (default: 4096)
-m, --matrix-dim1 <[min:]max>  matrix first-dimension range, level 2 & 3 (default: 4096)
-M, --matrix-dim2 <[min:]max>  matrix second-dimension range, level 2 & 3
                                (default: same as --matrix-dim1, i.e. square)
-t, --thread-count <[min:]max> number of BLAS threads (default: 1)
-s, --statistics               add stddev/min/max columns
-o, --output <file>            also save results to file
-f, --output-format <fmt>      csv or json (default: csv, or guessed from -o)
-h, --help                     show this help
```

A `[min:]max` range doubles from `min` to `max` (`256:4096` → 256, 512,
1024, 2048, 4096). A bare `max` runs just that one size.

```bash
# sweep vector size, with stddev/min/max, saved as CSV
bmb_ddot -v 1024:16384 -s -o results.csv

# 4 threads, either way works the same:
bmb_dgemm -t 4
OMP_NUM_THREADS=4 bmb_dgemm
```

If the thread-count flag and the backend's environment variable
(`OMP_NUM_THREADS` for OpenBLAS/NVPL/ArmPL, `BLIS_NUM_THREADS` for BLIS)
disagree, the flag wins and a warning explains why:

```
$ OMP_NUM_THREADS=4 bmb_dgemm -t 1
OMP_NUM_THREADS is ignored! Set to 4 but option -t is set to 1.
```

## Backends

Pick one at `configure` time — no code changes, they all share the same
CBLAS interface:

| Backend | `configure` flag |
| --- | --- |
| OpenBLAS *(default)* | *(none needed, or `--with-blas-backend=openblas`)* |
| BLIS | `--with-blas-backend=blis` |
| NVPL *(aarch64)* | `--with-blas-backend=nvpl` |
| ArmPL *(aarch64)* | `--with-blas-backend=armpl` |

All four build and pass `make check` in CI. Netlib isn't supported yet —
Debian/Ubuntu ships no CBLAS wrapper for it — and cuBLAS/rocBLAS aren't
planned. See [AGENTS.md](AGENTS.md) for the technical details and the
exact install steps `.github/workflows/ci.yml` uses for each backend.

`./configure --help` lists every option, including `--with-blas-libpath` for
a non-standard install location and the usual `CC`/`CFLAGS`/`LDFLAGS`.

## License

[Apache License 2.0](LICENSE).
