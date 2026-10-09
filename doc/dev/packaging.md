# Packaging

The Spack package, how it is tested, and how it follows a release. Start
at [AGENTS.md](../../AGENTS.md).

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
  netlib-lapack, nvpl-blas, armpl-gcc, flexiblas). Any other provider
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
| `test_man_pages` | the two pages, and an alias per benchmark |

Each was seen to fail: a corrupted result (`BMB_VERIFY_CORRUPT=1`), a
library expected under another prefix, an empty report, a missing alias
page, another version.

### In CI

The `spack` job of `ci.yml` runs `.github/spack-check.sh BLAS SRC` for
OpenBLAS, BLIS, Netlib and FlexiBLAS (NVPL and ArmPL are aarch64-only,
and their Spack packages download the vendor's binaries). For each, it
builds and tests:

- **this commit**, from the tarball `make dist` makes, through a Spack
  environment that develops it (`spack develop --path`): the recipe as
  it is, on the code as it is;
- **the latest release in the recipe**, as users get it: Spack fetches
  the tarball from GitHub and checks its sha256.

Spack (`SPACK_COMMIT`) and its package recipes (`SPACK_PACKAGES_COMMIT`)
are pinned to commits, with their release tag in a comment, as the
actions are: a tag can be moved. What Spack builds is cached, keyed on
both and on `packaging/spack`. A new Spack release is taken by moving the
two commits to its tags' (`gh api repos/spack/spack/git/ref/tags/vX.Y.Z`,
and the same for `spack/spack-packages`).
The BLIS and Netlib legs install the distribution's library as well, so
that the benchmarks loading it instead of Spack's would fail
`test_linked_library`.

To run the same here:

```bash
git clone --depth 1 -b v1.2.2 https://github.com/spack/spack.git ~/spack
. ~/spack/share/spack/setup-env.sh
spack repo update builtin --tag v2026.06.0   # the commits CI pins
spack repo add packaging/spack/spack_repo/blas_microbenchmark
(cd build && make dist && tar xf blas-microbenchmark-*.tar.gz -C /tmp)
.github/spack-check.sh openblas /tmp/blas-microbenchmark-<version>
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
