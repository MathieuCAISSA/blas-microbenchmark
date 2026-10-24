<h1 align="center">blas-microbenchmark</h1>

<p align="center">
  <b>How fast is each BLAS routine on this machine, with this library?</b><br>
  One small benchmark per routine, every result checked before it is timed,<br>
  and the libraries compared on one page of charts.
</p>

<p align="center">
  <a href="https://github.com/MathieuCAISSA/blas-microbenchmark/actions/workflows/ci.yml?query=branch%3Amain"><img alt="linux-tests" src="https://img.shields.io/github/actions/workflow/status/MathieuCAISSA/blas-microbenchmark/ci.yml?branch=main&label=linux-tests&logo=linux&logoColor=white"></a>
  <a href="https://codecov.io/gh/MathieuCAISSA/blas-microbenchmark"><img alt="codecov" src="https://codecov.io/gh/MathieuCAISSA/blas-microbenchmark/branch/main/graph/badge.svg"></a>
  <a href="https://scorecard.dev/viewer/?uri=github.com/MathieuCAISSA/blas-microbenchmark"><img alt="OpenSSF Scorecard" src="https://api.scorecard.dev/projects/github.com/MathieuCAISSA/blas-microbenchmark/badge"></a>
  <a href="https://www.bestpractices.dev/projects/15515"><img alt="OpenSSF Best Practices" src="https://www.bestpractices.dev/projects/15515/badge"></a>
  <a href="https://mathieucaissa.github.io/blas-microbenchmark/"><img alt="Documentation" src="https://img.shields.io/badge/docs-online-2160c4.svg"></a>
  <a href="LICENSE"><img alt="License: Apache 2.0" src="https://img.shields.io/badge/license-Apache%202.0-blue.svg"></a>
</p>

- **32 benchmarks**, one executable per BLAS routine (`bmb_dgemm`,
  `bmb_ddot`, …), levels 1 to 3, in single, double and complex
  precision. Think [osu-micro-benchmarks](https://mvapich.cse.ohio-state.edu/benchmarks/),
  for BLAS instead of MPI.
- **Numbers you can trust**: every point is checked against a reference
  before it is timed, so a library that computes the wrong thing stops
  the run instead of looking fast. The CPU, its frequency, the library,
  its version and the kernels it chose, and how the run was made, are
  recorded with every result.
- **Eight BLAS libraries**: OpenBLAS, BLIS, AMD AOCL-BLAS, Intel MKL,
  NVPL, ArmPL, Netlib, or all those FlexiBLAS can load, from one build.
- **One page of charts**, `bmb_report`: performance against size with the
  caches marked, libraries side by side, thread scaling, and how far
  repeated runs spread. A single HTML file, which opens offline.
- **Made for HPC sites**: a Spack package, a Docker image (Apptainer
  too), and ReFrame tests.

## Quick start

With a C compiler and a BLAS library (`libopenblas-dev` on Debian and
Ubuntu), take **`blas-microbenchmark-<version>.tar.gz`** from the
[latest release](https://github.com/MathieuCAISSA/blas-microbenchmark/releases/latest), then:

```bash
tar xf blas-microbenchmark-<version>.tar.gz && cd blas-microbenchmark-<version>
./configure && make && make check && sudo make install
export PATH="/usr/local/libexec/blas-microbenchmark/level3:$PATH"
bmb_dgemm -m 512:2048
```

```
$ bmb_dgemm -m 512:2048
# blas-microbenchmark 1.8.0
# backend: openblas (OpenBLAS 0.3.26 NO_LAPACKE DYNAMIC_ARCH NO_AFFINITY Haswell MAX_THREADS=64)
# kernels: Haswell
# verified: every point, against a reference, before it was timed
# layout: row-major
# cpu: Intel(R) Core(TM) Ultra 7 155U (14 logical CPUs, 1 NUMA node)
# caches: L1d 48K, L1i 64K, L2 2M, L3 12M
# os: Linux 6.6.87.1-microsoft-standard-WSL2 x86_64
# date: 2026-10-23T12:34:59Z
# affinity: CPUs 0-13
# command: bmb_dgemm -m 512:2048
# run: 0.2 s and 1 call of warm-up, 10 samples, batch size chosen per point, hot cache
# operands: aligned to 64 bytes, leading dimensions + 0
# compiler: gcc 13.3.0, CFLAGS -g -O2 -fopenmp
# routine: dgemm
Thread count    Matrix dim1 (M=K)   Matrix dim2 (N)     time [s]        GFLOP/s       median [s]      IQR [%]
1               512                 512                 0.006034826     44.481        0.011482770     33.9
1               1024                1024                0.044718863     48.022        0.046378110     19.0
1               2048                2048                0.381516047     45.031        0.423780287     4.3
```

Every benchmark takes the same options: the sizes to sweep (`-v` for
vectors, `-m` and `-M` for matrices), thread counts (`-t 1:8`), a file to
save to (`-o results.json`, or `.csv`). `bmb_dgemm --help` lists them;
**`man blas-microbenchmark`** explains everything else.

## Compare libraries: `bmb_report`

Run the same commands against each library, then make one page of them:

```bash
bmb_ddot  -v 1024:16777216           -o results/ddot-openblas.json
bmb_dgemm -m 128:2048 -t 1:8         -o results/dgemm-openblas.json
bmb_dgemv -m 256:4096 -M 256:4096    -o results/dgemv-openblas.json
# ... the same three from a build against BLIS, as *-blis.json
bmb_report results/*.json > report.html
firefox report.html
```

<p>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="doc/images/ddot-size-dark.png">
  <img src="doc/images/ddot-size-light.png" alt="ddot GFLOP/s against vector size, OpenBLAS and BLIS, with the L1d, L2 and L3 sizes marked" width="49%">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="doc/images/dgemm-ratio-dark.png">
  <img src="doc/images/dgemm-ratio-light.png" alt="dgemm, BLIS relative to OpenBLAS at 8 threads, between x0.45 and x1 depending on size" width="49%">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="doc/images/dgemm-threads-dark.png">
  <img src="doc/images/dgemm-threads-light.png" alt="dgemm GFLOP/s at 1, 2, 4 and 8 threads at 2048x2048, OpenBLAS and BLIS, with ideal scaling dashed" width="49%">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="doc/images/dgemv-shapes-dark.png">
  <img src="doc/images/dgemv-shapes-light.png" alt="dgemv heatmaps of GFLOP/s over M and N from 256 to 4096, OpenBLAS and BLIS side by side" width="49%">
</picture>
</p>

*The commands above, run on a laptop (Intel Core Ultra 7 155U, WSL2)
against OpenBLAS 0.3.26 and BLIS 0.9.0: what the page draws, not a verdict
on either library.*

Run each command several times, into separate files, and the page also
shows how far the runs spread, and which differences are beyond that
noise. `man bmb_report` has the details.

## The benchmarks

<!-- BEGIN routines (readme): written by src/c/routines.sh -->
| Level | Double | Single | Complex |
| --- | --- | --- | --- |
| **1** — vectors | `dasum` `daxpy` `dcopy` `ddot` `dnrm2` `dscal` `dswap` | `saxpy` `sdot` | `zdotc` |
| **2** — matrix × vector | `dgemv` `dger` `dsymv` `dsyr` `dsyr2` `dtrmv` `dtrsv` | `sgemv` | |
| **3** — matrix × matrix | `dgemm` `dsymm` `dsyrk` `dsyr2k` `dtrmm` `dtrsm` | `sgemm` `ssyrk` `strsm` | `cgemm` `zgemm` `zhemm` `zherk` `ztrsm` |
<!-- END routines (readme) -->

Each is `bmb_<routine>`. Level 1 and 2 report GFLOP/s and GB/s, level 3
GFLOP/s.

## Install

### From a release

The [Quick start](#quick-start) above. `make install` puts `bmb_report` in
`/usr/local/bin`, the manual pages in `/usr/local/share/man`, and the
benchmarks by BLAS level under `/usr/local/libexec/blas-microbenchmark`,
not on the `PATH`; to run them all by name:

```bash
BMB=/usr/local/libexec/blas-microbenchmark
export PATH="$BMB/level1:$BMB/level2:$BMB/level3:$PATH"
```

`./configure --with-blas-backend=…` picks the library ([Backends](#backends)).
On a cluster, it finds one loaded with `module load` (`OPENBLAS_ROOT`,
`BLIS_INCDIR`, …), or point at it with `--with-blas-incpath=DIR` and
`--with-blas-libpath=DIR`.

<details>
<summary><b>Check that the download is untouched</b></summary>

Each release tarball is attested by the workflow that built it. With the
[GitHub CLI](https://cli.github.com/):

```bash
gh attestation verify blas-microbenchmark-<version>.tar.gz --repo MathieuCAISSA/blas-microbenchmark
```

Or, from 1.3.0, with [cosign](https://docs.sigstore.dev/cosign/) and the
bundle attached to the release (signed by the release workflow, run for
the version's tag, or from `main` when a release's upload is run again):

```bash
cosign verify-blob-attestation --type slsaprovenance1 \
  --bundle blas-microbenchmark-<version>.tar.gz.sigstore.json \
  --certificate-oidc-issuer https://token.actions.githubusercontent.com \
  --certificate-identity-regexp '^https://github\.com/MathieuCAISSA/blas-microbenchmark/\.github/workflows/release\.yml@refs/(tags/v|heads/main$)' \
  blas-microbenchmark-<version>.tar.gz
```

</details>

### With Spack

```bash
spack repo add https://github.com/MathieuCAISSA/blas-microbenchmark.git
spack install blas-microbenchmark ^openblas
spack load blas-microbenchmark
```

The BLAS can be `^openblas`, `^blis`, `^amdblis`, `^intel-oneapi-mkl`,
`^nvpl-blas`, `^armpl-gcc`, `^netlib-lapack` or `^flexiblas`.

### In a container

From 1.6.0, each release's image is on `ghcr.io`, for x86_64, against
OpenBLAS (`latest`, `<version>`, `<version>-openblas`), BLIS
(`<version>-blis`) or Netlib (`<version>-netlib`). It writes to
`/results`, here the current directory:

```bash
IMAGE=ghcr.io/mathieucaissa/blas-microbenchmark:latest
gh attestation verify oci://$IMAGE --repo MathieuCAISSA/blas-microbenchmark
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/results" $IMAGE bmb_dgemm -m 64:1024 -o dgemm.json
apptainer exec docker://$IMAGE bmb_dgemm -m 64:1024 -o dgemm.json
```

<details>
<summary><b>Build the image yourself, with MKL, or for Apptainer</b></summary>

From a git clone, against OpenBLAS or the library `BLAS_BACKEND` names
(`blis`, `netlib`, or `mkl`, which is not published since it would
redistribute Intel's libraries):

```bash
docker build -f packaging/docker/Dockerfile --build-arg BLAS_BACKEND=openblas -t blas-microbenchmark .
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/results" blas-microbenchmark bmb_dgemm -m 64:1024 -o dgemm.json
```

Apptainer takes a built image too: save it where Docker is, then convert
it where Apptainer is, which writes in the current directory as you:

```bash
docker save -o blas-microbenchmark.tar blas-microbenchmark
apptainer build blas-microbenchmark.sif docker-archive://blas-microbenchmark.tar
apptainer exec blas-microbenchmark.sif bmb_dgemm -m 64:1024 -o dgemm.json
```

A container runs on the host's CPU, which is what the results record.
Docker's `--cpus` and `--cpuset-cpus` leave the benchmark fewer CPUs than
it sees, and `-t` beyond those gets a warning. Docker Desktop runs
containers in a virtual machine, whose own limits the numbers include.

</details>

### From git

Run `./autogen.sh` first (it needs autoconf, automake and libtool), then
as from a release. GitHub's *"Source code"* archives are git exports too.

## Backends

Chosen when building, and each one built and tested in CI:

| Library | `configure` flag | Spack |
| --- | --- | --- |
| OpenBLAS *(default)* | *(none)* | `^openblas` |
| BLIS | `--with-blas-backend=blis` | `^blis` |
| AMD AOCL-BLAS | `--with-blas-backend=aocl` | `^amdblis` |
| Intel MKL | `--with-blas-backend=mkl` | `^intel-oneapi-mkl` |
| NVPL *(aarch64)* | `--with-blas-backend=nvpl` | `^nvpl-blas` |
| ArmPL *(aarch64)* | `--with-blas-backend=armpl` | `^armpl-gcc` |
| Netlib reference | `--with-blas-backend=netlib` | `^netlib-lapack` |
| FlexiBLAS | `--with-blas-backend=flexiblas` | `^flexiblas` |

With FlexiBLAS, one build measures every library it can load
(`FLEXIBLAS=BLIS-OPENMP bmb_dgemm …`), each its own series in the report.
Fedora packages it; Debian and Ubuntu do not.

## In a test suite: ReFrame

Sites that check their systems with [ReFrame](https://reframe-hpc.readthedocs.io/)
get every benchmark as a ReFrame test, installed with them: its result
checked for sanity, its GFLOP/s and GB/s as performance variables,
compared with the site's references.

```bash
R=/usr/local/share/blas-microbenchmark/reframe   # with Spack: $(spack location -i blas-microbenchmark)/share/...
reframe -c $R/blas_microbenchmark.py -n bmb_run -r
reframe -c $R/blas_microbenchmark.py -n bmb_run -S level3_size=4096 -S reference_file=refs.json -r
bmb_report output/*/*/*/*/result.json > report.html
```

Sizes, threads, references, and building a release in each programming
environment: [packaging/reframe](packaging/reframe/README.md).

## Documentation

- **[The manual](https://mathieucaissa.github.io/blas-microbenchmark/)**,
  online or as `man blas-microbenchmark` and `man bmb_report`, with
  [what a run produces](https://mathieucaissa.github.io/blas-microbenchmark/outputs.html)
  (table, CSV, JSON, the report). Before installing:
  `man -l man/blas-microbenchmark.1` from the build directory.
- **[The wiki](https://github.com/MathieuCAISSA/blas-microbenchmark/wiki)**:
  results measured on real machines, and a FAQ.
- **[CHANGELOG.md](CHANGELOG.md)**, and the
  **[roadmap](https://github.com/users/MathieuCAISSA/projects/1)**.
- **Contributing**: [CONTRIBUTING.md](CONTRIBUTING.md), then
  [AGENTS.md](AGENTS.md) and [doc/dev/](doc/dev/); every test and CI job is
  on the [development page](https://mathieucaissa.github.io/blas-microbenchmark/development.html).

## Citing and license

If you use it in published work, please cite it: GitHub's *Cite this
repository* button gives the reference, from [CITATION.cff](CITATION.cff).
Released under the [Apache License 2.0](LICENSE).
