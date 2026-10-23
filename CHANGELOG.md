# Changelog

What changed in each release, for the people who use it. The full notes,
with examples, are on the
[releases page](https://github.com/MathieuCAISSA/blas-microbenchmark/releases).
Versions follow [semantic versioning](https://semver.org/): results from
releases with the same major version can be compared and read by the same
`bmb_report`.

## [1.8.0] - 2026-10-23

### Changed
- The default size depends on the level (#165): 10000000 at level 1 and
  5000 at level 2, past most last-level caches, so that their GB/s are
  the memory's; 2000 at level 3, under a second a dgemm call on one core
  (5 s a point, where 4096 took about 30 s). None is a power of two. A
  run that gave no size measures something else than in 1.7.0; the
  ReFrame tests' defaults are the same.
- Each thread count of a run starts with 0.2 s of warm-up calls
  (`-w`, `--warmup-time`; `-w 0` turns it off), so that a small point
  is not measured before the CPU has left its idle clock (#164).
- Operands are aligned to 64 bytes, where they were wherever `malloc`
  put them (16 bytes with glibc). A library sensitive to it can measure
  differently from 1.7.0 at the same size; `-a 16` comes close to before.

### Added
- Every result says which kernels the library's dispatch chose at run
  time (#162): OpenBLAS's core, BLIS's and AOCL-BLAS's configuration,
  MKL's code branch, `AUTO` where it names none, as on AMD CPUs
  (`# kernels:` in the text and CSV, `kernels` in the JSON, `Kernels:`
  in `-V`), and `bmb_report` shows it beside the library. Netlib, NVPL,
  ArmPL and FlexiBLAS do not say.
- `-T` (`--min-time`) takes samples past `-i` until they add up to a
  time, and `-e` (`--max-spread`) until their interquartile range is
  within a percentage of their median, capped by `-E` (`--max-time`,
  10 s by default) and 100000 samples (#164). The spread is the IQR, not
  the coefficient of variation, which one preempted sample keeps from
  settling. Every row says how many samples it took (`samples`, in the
  CSV and JSON, and in the table with `-T` or `-e`).
- `-a` (`--align`) and `-p` (`--ld-pad`): every operand is aligned, to 64
  bytes by default, and a matrix's leading dimension can be padded, to
  step a power-of-two size such as 4096 off the cache sets
  it would otherwise share (#163). The results say both (`# operands:`,
  `align` and `ld_pad` in the JSON), and `bmb_report` keeps a padded
  series apart.
- `-k cold` (`--cache cold`): each timed call finds its operands out of
  the caches, by rotating over enough copies of them that the others
  reach past twice the last-level cache (#160). The GB/s of a vector that
  fits in a cache then compare with STREAM's, not with that cache's.
  Hot, the default, is what the benchmarks always did. The results say
  which (`cache` in the JSON's run block), how many copies each point
  took (`copies`), and `bmb_report` keeps hot and cold series apart.
- Every result says how it was measured, so that it can be measured
  again (#161): the CPUs the process could run on, the command line, the
  warm-up calls, samples and batch size, the compiler and CFLAGS of the
  benchmarks, and the variables set among those that change what a BLAS
  library does (`OMP_PROC_BIND`, `OMP_PLACES`, `OPENBLAS_CORETYPE`,
  `MKL_ENABLE_INSTRUCTIONS`, ...). Comment lines in the table and the
  CSV, a `run` object in the JSON.
- The median and the quartiles of the samples, in every output, with or
  without `-s`: `median_s`, `q1_s` and `q3_s` in the CSV and JSON, the
  median and the interquartile range in % of it in the table. The
  batch size (`calls_per_sample`) is always in the CSV and JSON too.
  They come after `-s`'s columns, which stay where they were, so a CSV
  read by column position keeps its columns.
- `bmb_report` shows how each series was run (a Run column, the command
  lines in its tooltip), and draws each point's band from the fastest
  sample to the third quartile; the raw data has the quartiles.

## [1.7.0] - 2026-10-19

### Changed
- A build's benchmarks are in a directory per precision of their level,
  `single`, `double`, `complex` or `complex16`:
  `build/src/c/level3/double/bmb_dgemm`. A script that ran them from a
  build tree needs the new path. Installed, nothing moves
  (`<prefix>/libexec/blas-microbenchmark/level3/bmb_dgemm`). The sources
  are arranged the same way.

## [1.6.1] - 2026-10-19

### Added
- Each release carries its provenance as an in-toto file too,
  `<tarball>.intoto.jsonl`: the signed SLSA provenance of the
  `.sigstore.json` bundle, on its own. 1.3.0, 1.4.0 and 1.5.0, which
  were attested when they were built, now carry both, so `cosign` checks
  them as the README says; 1.6.0 carries its provenance too.

### Fixed
- A machine whose CPUs use more frequency governors than the result's
  field holds records those that fit, each whole: the last one was cut
  mid-name, in the text, CSV and JSON results alike.

## [1.6.0] - 2026-10-16

### Added
- The Docker image is published with each release, on
  `ghcr.io/mathieucaissa/blas-microbenchmark`, for x86_64, against
  OpenBLAS (`<version>`, `latest`, `<version>-openblas`), BLIS
  (`<version>-blis`) and Netlib (`<version>-netlib`), with an attestation
  of where it was built; see the README.
- Each release carries the attestation of where its tarball was built as
  a file, `<tarball>.sigstore.json`, which `cosign` checks without the
  GitHub CLI; see the README.
- The ReFrame tests are installed with the benchmarks, in
  `<prefix>/share/blas-microbenchmark/reframe`, and run that install with
  no option: `reframe -c <prefix>/share/blas-microbenchmark/reframe/blas_microbenchmark.py -n bmb_run -r`.
  The README has a section on them, and the man page lists them under
  FILES and EXAMPLES.
- Guides on the documentation site
  (<https://mathieucaissa.github.io/blas-microbenchmark/>): installing,
  a first run, comparing libraries, the BLAS libraries one by one, on an
  HPC cluster, and the ReFrame tests.

### Changed
- The documentation site is reorganised around a menu: Getting started,
  the guides, then the manual, a page per part (options, measuring,
  checking, threads, counters, output, environment, examples), each with
  its table of contents and its previous and next pages. It reads on a
  phone too.
- The man pages, `blas-microbenchmark(1)` and `bmb_report(1)`, give the
  site's address under SEE ALSO.

## [1.5.0] - 2026-10-15

### Added
- Six more benchmarks in single precision and complex: `bmb_strsm`,
  `bmb_ztrsm`, `bmb_ssyrk`, and the complex-only `bmb_zdotc`, `bmb_zherk`
  and `bmb_zhemm` (32 in all). A complex routine counts four times its
  real counterpart's operations; each is checked like the others, the
  Hermitian ones against a reference that, like BLAS, reads only the
  upper triangle and the real part of the diagonal.
- An Intel MKL backend (`--with-blas-backend=mkl`): oneAPI MKL from
  Intel's repository or a module (`MKLROOT`), or Debian and Ubuntu's
  `libmkl-dev`. `-t` sets MKL's BLAS thread count, above
  `MKL_DOMAIN_NUM_THREADS`; without it, `MKL_NUM_THREADS` then
  `OMP_NUM_THREADS`. The Spack package takes `^intel-oneapi-mkl`.
- An AMD AOCL-BLAS backend (`--with-blas-backend=aocl`): AMD's fork of
  BLIS, from AOCL or a module (`AOCL_ROOT`), linked through its threaded
  `libblis-mt`; its results say `aocl` and AOCL-BLAS's version. The
  Spack package takes `^amdblis`.
- A Spack package: `spack repo add` this repository's URL, then
  `spack install blas-microbenchmark ^openblas` (or `^blis`, `^amdblis`,
  `^netlib-lapack`, `^flexiblas`, `^nvpl-blas`, `^armpl-gcc`); `spack test
  run` checks the install, and `spack load` puts the benchmarks on `PATH`.
- A Dockerfile (`packaging/docker`): an image built from the source tree
  against OpenBLAS, BLIS, Netlib or MKL (`--build-arg BLAS_BACKEND=`),
  with the benchmarks on its `PATH`, the man pages and `bmb_report`,
  which Apptainer can convert; see the README.
- ReFrame tests (`packaging/reframe`): one per routine, on an installed
  copy or one built from a release tarball, verified, with their rates
  as performance variables and a site's references read from a JSON
  file; see `packaging/reframe/README.md`.
- A warning when `-t` asks for more threads than the CPUs the process may
  run on: its CPU affinity (`taskset`, a Slurm job, `docker --cpuset-cpus`)
  or its cgroup's CPU quota (`docker --cpus`). They would take turns, and
  the timings would measure that.

### Fixed
- `--with-blas-incpath` and `--with-blas-libpath` lost to a
  distribution's library found by `configure` (BLIS, Netlib, NVPL, ArmPL,
  FlexiBLAS): with both installed, the system's library was linked, with
  the headers asked for. The options now come first in the search, ahead
  of a module's variables too.
- In a build directory where `configure` was rerun with another version,
  `bmb_report --version` kept the old one. A release tarball, always
  built from scratch, was not affected.

## [1.4.0] - 2026-10-08

### Added
- Single-precision and complex benchmarks: `bmb_saxpy`, `bmb_sdot`,
  `bmb_sgemv`, `bmb_sgemm`, `bmb_cgemm` and `bmb_zgemm`, with the same
  options and checks as the others. A complex multiply-add counts as 8
  real operations, so cgemm and zgemm compare with dgemm in GFLOP/s.
  Their results are checked against a reference in double, within a
  rounding bound for their precision; the report lists each operation's
  precisions side by side (sgemm, dgemm, cgemm, zgemm).
- The report shows the history of a library measured in three versions
  or more on the same machine: for each routine, its performance at the
  largest size all versions measured, oldest to newest, with a version
  slower than the one before beyond noise marked "slower".
- `blas-microbenchmark(1)` has a HARDWARE COUNTERS section: how to read
  them with `perf stat` or `likwid-perfctr` around a benchmark, and how
  two runs that differ only in `-i` give what one call costs.
- A FlexiBLAS backend (`--with-blas-backend=flexiblas`): one build
  measures every library FlexiBLAS can load, chosen at run time with
  `FLEXIBLAS=...`. The results name the library loaded (backend
  `flexiblas/openblas-openmp`), so the report shows each as its own
  series; the thread-count variables are that library's.
- `-L`/`--layout row|col`: store the matrices column-major and call
  CBLAS that way, as Fortran and LAPACK do, instead of the default
  row-major. A library may take another code path for each. The routines
  with a matrix record it (`# layout:`, `"layout"` in the JSON), check
  their results in either order, and the report keeps the two as two
  series.

### Fixed
- An explicit `-b` above 1 batched dtrmv, dtrsv, dtrmm and dtrsm, whose
  operand has to be restored before every call: it drifted inside the
  batch, and dtrmv came out 1.5× too fast. They now stay at one call per
  sample, with a warning that `-b` was ignored.

## [1.3.0] - 2026-10-05

### Added
- Repeated runs in the report: run the same command several times, each
  into its own file, and every point keeps all its runs. The curve still
  shows the fastest; a band shows the spread to the slowest, and the
  comparison with the reference tests each difference against that
  spread (Mann-Whitney U, two-sided, p < 0.05): a point within the noise
  is drawn hollow. It takes 4 runs of each, or 3 against 5; with fewer,
  the page says so. The raw data has a row per run.
- `-s` adds the median of the samples, as the last column (`median_s`
  in the JSON), so that a CSV read by position keeps its other columns.
- The CPU frequency governor and turbo are recorded with every result
  (`# frequency:`, and `governor` and `turbo` in the JSON `machine`
  object) and shown in the report's provenance table. When they let the
  frequency change during a run, a warning says so and gives the
  commands that fix it; the run goes on. `--version` says what the
  system exposes.
- `.github/REVIEWS.md`, the checklist every pull request is reviewed against.
- A code of conduct, the Contributor Covenant 3.0
  (`.github/CODE_OF_CONDUCT.md`); reports go through GitHub.
- Release tarballs carry a signed attestation of where they were built:
  `gh attestation verify blas-microbenchmark-<version>.tar.gz --repo
  MathieuCAISSA/blas-microbenchmark` checks the file came from this
  repository's release workflow, untouched.
- `.editorconfig`, for contributors' editors; `make check` holds every
  file in git to it.

### Changed
- `--verify` is on by default, as the check is in STREAM and HPL: every
  point is checked against a reference before it is timed, and a wrong
  result stops the run with exit status 2. `-C`/`--no-verify` turns it
  off. Every output now says `# verified:` unless `-C` was given.
- For contributors: the developer guide is split. `AGENTS.md` keeps what
  every change needs and points to one file per part of the project in
  `doc/dev/`; `make check` checks every link between them. The project's
  own tests moved to `tests/`, and the code of conduct, the security
  policy and the review checklist to `.github/`, in the repository and
  in the tarball.

### Fixed
- When memory ran out during `--verify`, pointers were computed from the
  failed allocation before it was checked: undefined behaviour, now
  gone. Found by cppcheck, which CI now runs, along with CodeQL.
- A thread-count environment variable that is not a whole number of
  threads (`OMP_NUM_THREADS=-2`, `99999999999`, `" 3"`) labelled the run
  with a wrong count, such as 4294967294 threads. It is now ignored with
  a warning, and the next variable is read.

## [1.2.0] - 2026-10-04

### Added
- `-c`/`--verify`: before timing each point, check that the library
  computes the right result there, against a reference computed
  independently (by random projection for level 3, at the cost of a level
  2 call). A wrong result stops the run with exit status 2 and writes no
  results file; verified results say so (`# verified:`, `"verified": true`).
- `bmb_report` shows which results were verified: a Verified column in
  the table of where the results came from (yes, no, or "1 of 3 files" for
  a series merged from several) and in the raw data, once any result was
  measured with `--verify`.
- `CONTRIBUTING.md`, `SECURITY.md`, `CITATION.cff` and this changelog,
  shipped in the release tarball too; issue and pull request templates.
- Releases carry a `SHA256SUMS` file for the tarball.

## [1.1.2] - 2026-10-04

### Fixed
- `make check` failed with a `--prefix` of more than about 190 characters:
  `test_man_install.sh` looked for the install path on one line, which it no
  longer is since 1.1.1. The programs and pages are the same as in 1.1.1.

## [1.1.1] - 2026-10-04

### Fixed
- `man blas-microbenchmark`: a long install path now wraps after a `/`
  instead of running past the edge, with groff warnings above the page.
- `man blas-microbenchmark`: the hyphen of a `-dev` version is escaped, so
  it is not rendered as a typographic hyphen. Releases were not affected.

## [1.1.0] - 2026-10-04

### Added
- Manual pages, installed under `<prefix>/share/man`:
  `blas-microbenchmark(1)`, also opened by `man bmb_<routine>`, and
  `bmb_report(1)`. `--help` points at them.
- A documentation site generated from them, with the outputs of real runs
  and the tests and CI described by their own sources:
  <https://mathieucaissa.github.io/blas-microbenchmark/>.

### Changed
- The README keeps what is needed to get started; the reference is in the
  manual pages.

## [1.0.0] - 2026-10-02

### Added
- `bmb_report`: any number of JSON results turned into one self-contained
  HTML page of charts — performance and bandwidth against size with the
  caches marked, each library against a reference, thread scaling, a
  heatmap of shapes, and the raw data.
- Every result records the machine: CPU, logical CPUs, NUMA nodes, caches,
  OS and date (never the hostname).
- `--label` (`-l`), to tell apart runs no automatic field can.
- JSON names the two dimensions (`dim1_label`, `dim2_label`).

### Changed
- **Without `-M`, level 2 and 3 sweeps measure square matrices only**, as
  `--help` always said; give both `-m` and `-M` for the grid.
- Text and CSV output start with more `#` comment lines.
- `bmb_report` reads 1.x results only, and says why it refuses the others.

## [0.6.1] - 2026-09-23

### Changed
- Documentation only: the install instructions and the reasons for the
  install layout.

## [0.6.0] - 2026-09-23

### Changed
- **How a number is measured**: each sample times a batch of calls, sized
  automatically, and the reported time is the fastest sample, not the mean.
  Numbers from 0.5.0 and earlier are not comparable.
- `-s` reports the mean, the standard deviation, the slowest sample and the
  batch size.
- CSV column names lost their trailing underscore.

### Added
- Every result names the backend and the BLAS library's own version string.
- `--batch`, and `-V`/`--version`.

### Fixed
- A size whose square overflowed `size_t` was accepted.
- A failed write of the results went unnoticed.

## [0.5.0] - 2026-09-22

### Added
- GFLOP/s for every routine that does arithmetic, GB/s for levels 1 and 2.
- Sweeps with a linear step (`min:max:step`) or an explicit list of sizes.

## [0.4.0] - 2026-09-22

### Fixed
- The end of a size range was never measured.
- Restoring an operand was timed along with the routine.
- `OMP_NUM_THREADS` was ignored by the OpenBLAS backend.
- Timings under half a microsecond printed as zero.
- Netlib against a static `libblas.a` did not link.

## [0.3.0] - 2026-09-22

### Changed
- The benchmarks install grouped by BLAS level under
  `<prefix>/libexec/blas-microbenchmark/level{1,2,3}`.

## [0.2.0] - 2026-09-22

### Added
- The Netlib reference BLAS backend, with a CBLAS layer of its own.
- `--with-blas-incpath`, and BLAS libraries found through the variables a
  cluster module sets.

## [0.1.1] - 2026-09-22

### Changed
- `configure` finds a BLAS loaded by a module; documentation rewritten.

## [0.1.0] - 2026-09-22

### Added
- 20 double-precision BLAS routines, levels 1 to 3, one executable each,
  with text, CSV and JSON output.
- OpenBLAS, BLIS, NVPL and ArmPL backends.

[1.8.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.7.0...v1.8.0
[1.7.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.6.1...v1.7.0
[1.6.1]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.6.0...v1.6.1
[1.6.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.5.0...v1.6.0
[1.5.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.4.0...v1.5.0
[1.4.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.3.0...v1.4.0
[1.3.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.1.2...v1.2.0
[1.1.2]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.1.1...v1.1.2
[1.1.1]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v0.6.1...v1.0.0
[0.6.1]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v0.1.1...v0.2.0
[0.1.1]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/MathieuCAISSA/blas-microbenchmark/releases/tag/v0.1.0
