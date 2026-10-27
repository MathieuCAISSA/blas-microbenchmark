# Packaging

The Spack package, the Docker image and the ReFrame tests: how each is
built and tested, and how they follow a release. Start at
[AGENTS.md](../../AGENTS.md).

## The Spack package

On HPC clusters, software comes from Spack rather than from tarballs.
`packaging/spack/` is a Spack package repository, namespace
`blas_microbenchmark`, laid out as Spack 1.x expects
(`spack_repo/<namespace>/packages/<package>/package.py`, with a
`repo.yaml`). It holds one package, `blas-microbenchmark`.
`spack-repo-index.yaml`, at the top of the repository because that is
where Spack looks in a clone, tells Spack where the package repository
is when it is added by its git URL:

```bash
spack repo add https://github.com/MathieuCAISSA/blas-microbenchmark.git
spack install blas-microbenchmark ^openblas
spack load blas-microbenchmark      # the three level directories on PATH
```

From a checkout, `spack repo add packaging/spack/spack_repo/blas_microbenchmark`
does the same with the files as they are.

What `package.py` does:

- **Versions.** It builds from the release tarball, which has its
  `configure`, so no Autotools are needed; `@main` builds from git and
  runs autoreconf.
- **The BLAS.** It depends on the `blas` virtual and maps the provider
  Spack chose to `--with-blas-backend` (`BACKENDS`: openblas, blis,
  amdblis, with `+cblas threads=openmp ~ilp64`, netlib-lapack, nvpl-blas,
  armpl-gcc, flexiblas, intel-oneapi-mkl, the
  last with `+gfortran threads=openmp`, the interface and threading
  layer `configure` links). Any other provider
  is refused at concretization, with that list: `configure` has no
  backend for it. A backend added to `configure.ac` goes into
  `BACKENDS` too.
- **Paths.** It passes the provider's library directory, and the
  directory holding its `cblas.h`, which several providers install in a
  subdirectory (`include/blis`, `include/flexiblas`), as
  `--with-blas-libpath` and `--with-blas-incpath`. Netlib needs no
  `cblas.h`: its C interface is ours (`src/c/netlib`).
- **Running them.** `spack load` puts `libexec/blas-microbenchmark/level{1,2,3}`
  on `PATH`, as osu-micro-benchmarks' package does.

Spack's compiler wrapper puts the directories of the Spack store ahead
of any other `-L` and rpath, so the library Spack built wins over a
distribution's even with a `configure` that put the distribution's
first (#58, fixed for 1.5.0).

### Its tests

`spack test run blas-microbenchmark` runs the package's `test_*`
methods on an installed spec:

| Test | What it checks |
|---|---|
| `test_version` | `bmb_dgemm --version` names the version and the backend of the spec |
| `test_linked_library` | `bmb_dgemm` loads the BLAS of the spec (`ldd`), not another one installed: what a library named differently in Spack and in the distribution would cause |
| `test_benchmarks` | every benchmark, at a tiny size, says its result was verified |
| `test_report` | `bmb_report` turns a result file into a page carrying it |
| `test_bmb_suite` | `bmb_suite` runs two benchmarks of the install into a directory, which `bmb_report` takes |
| `test_man_pages` | the three pages, and an alias per benchmark |

Each was seen to fail: a corrupted result (`BMB_VERIFY_CORRUPT=1`), a
library expected under another prefix, an empty report, a missing alias
page, another version.

### In CI

The `spack` job of `ci.yml` runs `.github/scripts/spack-check.sh BLAS SRC` for
OpenBLAS, BLIS, AOCL-BLAS, Netlib, FlexiBLAS and MKL (NVPL and ArmPL are
aarch64-only, and their Spack packages download the vendor's binaries).
MKL is Intel's apt package, declared to Spack as an external
(`/opt/intel/oneapi`): Spack's own package would download Intel's
installer, gigabytes, on every cache miss. For each, it builds and
tests:

- **this commit**, from the tarball `make dist` makes, through a Spack
  environment that develops it (`spack develop --path`): the recipe as
  it is, on the code as it is;
- **the latest release in the recipe**, as users get it: Spack fetches
  the tarball from GitHub and checks its sha256.

Spack (`SPACK_COMMIT`) and its package recipes (`SPACK_PACKAGES_COMMIT`)
are pinned to commits, with their release tag in a comment, as the
actions are: a tag can be moved. What Spack builds is cached, keyed on
both and on `packaging/spack`, and for the MKL leg on the MKL version
Intel's repository installed: the package is an external there, and
installs built against an MKL no longer on the runner would still be
tested, and fail (#139). A new Spack release is taken by moving the
two commits to its tags' (`gh api repos/spack/spack/git/ref/tags/vX.Y.Z`,
and the same for `spack/spack-packages`).
The BLIS, AOCL-BLAS and Netlib legs install the distribution's library
(BLIS's for AOCL-BLAS, its fork) as well, so
that the benchmarks loading it instead of Spack's would fail
`test_linked_library`.

To run the same here:

```bash
git clone --depth 1 -b v1.2.2 https://github.com/spack/spack.git ~/spack
. ~/spack/share/spack/setup-env.sh
spack repo update builtin --tag v2026.06.0   # the commits CI pins
spack repo add packaging/spack/spack_repo/blas_microbenchmark
(cd build && make dist && tar xf blas-microbenchmark-*.tar.gz -C /tmp)
.github/scripts/spack-check.sh openblas /tmp/blas-microbenchmark-<version>
```

### After a release

The new version's line goes into `package.py` once the tarball is
published, since its checksum is the published one's: in the pull
request that goes back to development ([release](release.md), step 7),

```python
version("X.Y.Z", sha256="<from the release's SHA256SUMS>")
```

above the older ones. The `spack` job then builds that version from
GitHub, which also checks the checksum.

### Upstream

The goal is the package in Spack's own repository,
[spack/spack-packages](https://github.com/spack/spack-packages), where
`spack install blas-microbenchmark` needs no `spack repo add`. The
`package.py` here follows its conventions (the copyright header, the
`spack_repo.builtin` imports, `maintainers`, `license`) so that it can
be copied there as is; their CI then runs its own checks on it.
This copy stays the reference: it is tested here against every change,
and a version is added here first.

## The Docker image

`packaging/docker/Dockerfile` builds an image from the source tree,
against the BLAS that `BLAS_BACKEND` names: `openblas` (the default),
`blis`, `netlib` or `mkl`. The README gives the commands, for Docker and
Apptainer. It is built from the top of the repository
(`docker build -f packaging/docker/Dockerfile .`), and
`Dockerfile.dockerignore` beside it, which Docker reads for that
Dockerfile instead of a `.dockerignore` at the top, keeps out the
history, the build directories and what `autoreconf` makes again.

- **Two stages.** The first installs Autotools, a compiler and the
  `-dev` package, runs `autoreconf -fi`, `configure`, `make` and
  `make install DESTDIR=/stage`; the second installs only the run-time
  library and copies `/stage` in. A table in each stage names the
  packages:

  | `BLAS_BACKEND` | built with | runs with |
  |---|---|---|
  | `openblas` | `libopenblas-dev` | `libopenblas0-pthread` |
  | `blis` | `libblis-openmp-dev` | `libblis4-openmp` |
  | `netlib` | `libblas-dev` | `libblas3` |
  | `mkl` | Intel's repository (`.github/scripts/install-oneapi-mkl.sh`) | `libgomp1`, and MKL's libraries copied |

  FlexiBLAS and AOCL-BLAS are not there: Ubuntu packages neither
  (AOCL-BLAS is built from AMD's sources, as the `aocl` CI job does).
  NVPL and ArmPL are aarch64-only, and the image is x86_64.
- **MKL** is not a package the final stage can install without
  gigabytes, so its libraries are copied, where the benchmarks'
  RUNPATH looks (`/opt/intel/oneapi/mkl/latest/lib`): the GNU OpenMP
  layer, the core, and the per-CPU kernels (`libmkl_def`, `mc3`,
  `avx*`), which the core opens at run time and `ldd` does not list.
  Intel's licence files go with them, in
  `/usr/local/share/doc/intel-oneapi-mkl`. A library missing there
  fails the image's own check (below) only on a CPU that needs it, so
  a new MKL that renames its kernels needs the list updated.
- **A tree configured in place** (`./configure` at the top rather than
  in `build/`) has a `config.status` and Makefiles that would build into
  the image instead of the sources; the build refuses it and says to run
  `make distclean`.
- **The checks it makes itself.** The final stage runs one tiny,
  verified benchmark per level (`bmb_zherk` among them): a library the
  benchmarks load but the stage lacks fails the build, not the user's
  first run.
- **Running it.** The benchmarks' three directories are on `PATH`. It
  runs as `bmb` (uid and gid 1001, numeric in `USER`, as hadolint asks),
  not root, in `/results`; `--user "$(id -u):$(id -g)"`
  makes what it writes to a mounted `/results` the host user's. man-db
  is installed, and Ubuntu's minimized image's stub in place of `man` is
  removed, so that `man blas-microbenchmark` works.
- **Pins.** The base image is pinned by digest in both `FROM` lines;
  Dependabot proposes the new digest (`.github/dependabot.yml`). The
  distribution's packages are whatever its archive holds, as on the CI
  runners.
- **A CPU quota.** In a container, `/proc/cpuinfo` and `sysconf` show
  the host's CPUs. What the process may use is read from its affinity
  and from its cgroup's `cpu.max` (`bmb_machine.c`; see
  [backends](backends.md)), and `-t` beyond it gets a warning.

### In CI

The `docker` job builds the image for each of the four backends, then
runs, from it: every benchmark with `--verify`, in both storage orders,
on 1 and 2 threads; `man`; a result written to a mounted directory as
the runner's user, turned into a page by `bmb_report`; and the warning
under `--cpus 1` and `--cpuset-cpus 0,1`, and its absence under
`--cpus 2` with `-t 2`. Once (the `openblas` leg), hadolint checks the
Dockerfile, and Apptainer converts the saved image to a SIF file and
runs a benchmark and `bmb_report` from it, as the README says to.

### Publishing

From 1.6.0 (#108), `release.yml`'s `image` job pushes the image to
`ghcr.io/mathieucaissa/blas-microbenchmark` on each version tag, for
OpenBLAS, BLIS and Netlib: `X.Y.Z-<backend>` for each, and `X.Y.Z` and
`latest` for OpenBLAS (`latest` is the last version tagged, so a patch
release of an older series would move it back). MKL is not published:
its image would redistribute Intel's libraries, under Intel's licence.

- **Attested** like the tarball (`actions/attest`), the attestation
  pushed to the registry beside the image; `gh attestation verify
  oci://…` checks it, as the README says.
- **Checked as a user gets it**: every local copy removed, the image
  pulled back by its digest, run, and its attestation verified, in the
  same job.
- **On a pull request** that changes `release.yml`, the job builds and
  runs each image, and pushes nothing. The `docker` job of `ci.yml`
  remains the one that tests the Dockerfile on every change.
- **Visibility**: a package created by a workflow starts private on a
  personal account. After the first release that pushes it (1.6.0), its
  settings (Packages, `blas-microbenchmark`, Package settings) must make
  it public, once; `release.md` says so in step 6.
- It is the publishing workflow OpenSSF Scorecard's `Packaging` check
  looks for (`docker … push`).

## The ReFrame tests

`packaging/reframe/blas_microbenchmark.py` is a ReFrame test file;
[its README](../../packaging/reframe/README.md) is for the sites that run
it. Both are in the tarball, and `make install` puts them in
`$(pkgdatadir)/reframe` (`Makefile.am`), so that a site has them with
the benchmarks; the man page's FILES and EXAMPLES point there. What is
not obvious in it:

- **The routine list** (`ROUTINES`, with each routine's level) is the
  test's parameter, so ReFrame needs it before anything is installed.
  `src/c/routines.sh` writes it from `src/c/routines.txt`, the table of
  routines, as it writes the other lists of them (#199).
- **One size, one point, per test.** A ReFrame performance variable is
  one number, so a test runs a single size (per level, set with `-S`)
  and a single thread count, and reads its rates from `result.json`.
  Which rates there are is read from that file before the performance
  stage, so dcopy reports no GFLOP/s rather than a missing one.
- **Sanity is `--verify`**: the line `# verified: every point` in the
  output, besides the exit status, so a site that adds `-C` to its
  options sees the test fail rather than pass unchecked.
- **References come from a JSON file**, not from a subclass, so a site
  writes no Python. They are read at init: ReFrame resolves them for the
  partition before the test runs. `reference` is assigned whole, as a
  dict: ReFrame turns it into a `ScopedDict` on assignment, where
  `reference['generic:default'] = ...` would take `generic` for a scope.
- **`prefix` is ReFrame's** (the test file's directory), hence
  `install_prefix`. `bmb_run` uses it to find the install it belongs to:
  run from `<prefix>/share/blas-microbenchmark/reframe`, it takes the
  benchmarks under `<prefix>/libexec/blas-microbenchmark`, so that the
  command in the README and the man page needs no option.
- **The tarball** is downloaded with the release's `SHA256SUMS` and
  checked, unpacked into `src/` whatever its directory is called, and
  built in `src/build`, given as `configuredir` and `builddir`: ReFrame
  sets `srcdir` itself, from `sourcepath`. A routine newer than the
  version built is skipped, not failed.
- **Its default version** is the latest release: step 7 of [the
  release](release.md) moves it, with the Spack package's.

### In CI

The `reframe` job installs ReFrame (pinned to a release) in a venv,
builds and installs this commit with OpenBLAS, then runs on ReFrame's
generic system:

- `-l`'s routines against the benchmarks installed;
- `bmb_run`, every routine, at small sizes on 2 threads, with a
  reference file that dgemm and dcopy meet; the report is read for each
  routine's rates (dcopy GB/s only, dgemm GFLOP/s only, ddot both) and
  the reference applied;
- a reference no machine meets, and `-C`, each failing the test, in the
  performance and the sanity stage respectively;
- `bmb_build_run` on this commit's tarball (`make dist`);
- `bmb_report` on every `result.json` they left.

ReFrame skips a test file it cannot load with a warning and exit status
0, so each step counts the tests that ran, rather than trust the status.
