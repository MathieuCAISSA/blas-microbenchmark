# Changelog

What changed in each release, for the people who use it. The full notes,
with examples, are on the
[releases page](https://github.com/MathieuCAISSA/blas-microbenchmark/releases).
Versions follow [semantic versioning](https://semver.org/): results from
releases with the same major version can be compared and read by the same
`bmb_report`.

## [Unreleased]

### Added
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

[Unreleased]: https://github.com/MathieuCAISSA/blas-microbenchmark/compare/v1.3.0...HEAD
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
