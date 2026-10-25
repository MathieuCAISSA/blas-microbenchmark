# Backends

How each BLAS library is found, linked and named. Start at
[AGENTS.md](../../AGENTS.md).

| Backend | `configure` flag | Verified |
| --- | --- | --- |
| OpenBLAS | `--with-blas-backend=openblas` (default via `auto`) | CI, `openblas` job |
| BLIS | `--with-blas-backend=blis` | CI, `blis` job |
| AOCL-BLAS | `--with-blas-backend=aocl` | CI, `aocl` job (built from AMD's sources) |
| Netlib | `--with-blas-backend=netlib` | CI, `netlib` job |
| NVPL | `--with-blas-backend=nvpl` | CI, `nvpl` job (`ubuntu-24.04-arm`) |
| ArmPL | `--with-blas-backend=armpl` | CI, `armpl` job (`ubuntu-24.04-arm`) |
| FlexiBLAS | `--with-blas-backend=flexiblas` | CI, `flexiblas` job (Fedora container), under OpenBLAS, BLIS and Netlib |
| Intel MKL | `--with-blas-backend=mkl` | CI, `mkl` job (oneAPI from Intel's apt repository) |
| GPU libraries (cuBLAS, rocBLAS, MAGMA, …) | — | Planned for 2.0 (#87) |

Every backend is reached through the CBLAS interface, so the same
`bmb_<routine>.c` files link against whichever one `configure` picks; only
`configure.ac`'s detection logic and `bmb_threads.c`'s thread-control
dispatch differ per backend.

Each backend also has to be *nameable*: `configure` resolves the selected
one into `BMB_BLAS_BACKEND` (`auto` becomes whatever `AC_SEARCH_LIBS`
actually linked, since "auto" is not an answer anyone can act on later),
and `BMB_CHECK_BLAS_VERSION_API` probes the call that library exposes its
own version string through. `bmb_build.c` turns both into the provenance
block every result carries. A new backend must set the name; the version
string is a bonus where the library has one (OpenBLAS's
`openblas_get_config()`, BLIS's `bli_info_get_version_str()`) and NULL
where it does not. The kernels its dispatch chose at run time are a
bonus too (#162, `bmb_build_kernels()`): OpenBLAS's
`openblas_get_corename()`, BLIS's and AOCL-BLAS's
`bli_arch_string(bli_arch_query_id())`, MKL's
`mkl_cbwr_get_auto_branch()`, which follows `MKL_ENABLE_INSTRUCTIONS`
(`AUTO` where MKL names no branch, as on AMD CPUs, #187), each detected
by `configure`. Whether a build has one is fixed when it is built, so
`bmb_build.h` defines `BMB_BUILD_HAS_KERNELS` and the outputs test it in
the preprocessor; it is undefined for Netlib, NVPL and ArmPL, which have
no such call, and for FlexiBLAS, which loads its library where the
benchmark cannot reach it. Debian's BLIS 0.9 aborts with any
`BLIS_ARCH_TYPE` set, so only OpenBLAS's and MKL's overrides are tested
(`test_bmb_kernels.sh`).

Batched GEMM (#159) is MKL's signature everywhere it exists:
`cblas_?gemm_batch` (MKL, BLIS, AOCL-BLAS, NVPL, OpenBLAS from 0.3.28)
and `cblas_?gemm_batch_strided` (MKL, NVPL, OpenBLAS from 0.3.31).
`configure` defines `HAVE_CBLAS_GEMM_BATCH` and
`HAVE_CBLAS_GEMM_BATCH_STRIDED` when the CBLAS header declares the four
precisions' calls and the library links them: a call taking arrays must
not go through an implicit declaration. BLIS's prototypes differ from
MKL's in their constness and integer type, so `bmb_gemm.c` passes the
arrays as `void *`. Netlib has neither, and a build without one skips
the mode (exit 77) rather than measure something else; the `mkl` job is
where `strided` runs, the `blis` job `group` (`test_bmb_gemm_batch.sh`).

The machine side of the provenance lives in `bmb_machine.c`, probed once per
run: CPU model, logical CPUs, NUMA nodes, cpu0's caches, the CPU frequency
governor and turbo, `uname`, and the date. Three rules there:

- **Every field is best effort and omitted when unreadable** — never
  guessed, never written empty. A container without sysfs cache entries is
  a legitimate "unknown", which is why `test_bmb_machine` checks only what
  every Linux box guarantees plus the sanity of whatever was recorded on
  the real machine, and exact values on the fake trees in
  `src/c/common/fixtures/machine/`.
- **On aarch64 there is no `model name`** in `/proc/cpuinfo`, only
  implementer and part codes, so the probe records them raw:
  `implementer 0x41, part 0xd49` on GitHub's arm64 runners (an ARM
  Neoverse-N2). **Never change that form.** The CPU string is part of the
  report's series key, and rewriting it — even into a nicer name — would
  stop files from the same machine merging across versions. Translating
  codes into names is the report's job, for display only (#1, decision 4).
  The `Provenance` step of each CI job prints what every runner reports.
- **No hostname, ever** (#1, decision 4). Reports get shared, and on a
  cluster identical nodes have different names.

**The frequency** (#3) is the governor of every CPU that has one
(`cpuN/cpufreq/scaling_governor`; the distinct ones, sorted and joined by
`/`, when they differ) and turbo, read from `intel_pstate/no_turbo` or,
for acpi-cpufreq and amd-pstate, `cpufreq/boost`. When either lets the
frequency move during a run, the benchmark warns on stderr with the
commands that fix it, and runs anyway: a laptop still gets numbers, and
the provenance says under what conditions. Neither is part of the
report's series key, so files from versions that did not record them
still merge; the report lists each condition a series' files ran under
instead. Virtual machines, WSL and most CI runners expose neither, which
`bmb_<routine> --version` says ("Frequency: not exposed by this system").

**The CPUs the process may use** (#88) are not the logical CPUs: in a
container, a Slurm job or under `taskset`, `sysconf` still counts the
host's. The probe reads the affinity (`Cpus_allowed_list` in
`/proc/self/status`: taskset, cpusets, Slurm, `docker --cpuset-cpus`)
and caps it by the cgroup v2 CPU quota (`cpu.max`: `docker --cpus`,
systemd's `CPUQuota`) of the process's cgroup and of every one above it,
since a parent's quota binds its children; a quota of 2.5 CPUs allows 3
threads at once. In a container with its own cgroup namespace, which
Docker gives by default, `/proc/self/cgroup` says `0::/` and the
container's quota is the root's `cpu.max`. cgroup v1 quotas are not
read. The count is not recorded in the results, only compared with the
largest `-t` before the run (`bmb_threads_check_usable()`): more threads
than that get a warning, except under a backend that takes no thread
count (Netlib, also when FlexiBLAS loaded it).

**`BMB_MACHINE_ROOT=DIR`** makes `bmb_machine()` probe `DIR/proc` and
`DIR/sys` instead of the real ones, as `test_bmb_machine` does through
`bmb_machine_probe_at()`. It is a test hook, so that the output and the
warnings can be checked against the fixtures in
`src/c/common/fixtures/machine/` (`test_bmb_output_formats.sh`,
`test_bmb_threads_env.sh`, `test_bmb_cache.sh`,
`test_bmb_alloc_failures.sh`, `test_bmb_bench`, and the report's render
test); it is not in
the man page. The logical CPU count,
the OS and the date still come from the real system.

`--label` is the escape hatch for what no probe can see. It goes on a
comment line of the text and CSV output, so it refuses control characters:
a newline in it would turn the rest of that line into data rows.

**Netlib** is the exception in how it gets that interface. Reference BLAS
is a Fortran library; some distributions bundle a CBLAS layer in it
(Debian/Ubuntu do — declared by `cblas-netlib.h`, *not* `cblas.h`, which is
why a plain search for `cblas.h` finds nothing and why this guide previously
claimed no such layer existed at all), but source builds and cluster
modules often have none. So `src/c/netlib/` provides the `cblas_*` entry
points itself, forwarding to the Fortran symbols, and that directory goes
on the include path ahead of everything else only for this backend, so its
`cblas.h` never shadows a real one. One code path, works either way.

Two things that backend gets wrong easily:

- Plain `-lblas` on Debian/Ubuntu goes through `update-alternatives` and
  normally resolves to *OpenBLAS*, silently benchmarking the wrong library.
  `configure` therefore looks for the reference build's own directory
  (`/usr/lib/*/blas`) first. If you touch that probe, check what
  `ldd src/c/level3/double/bmb_dgemm` actually resolves to.
- The shim translates row-major CBLAS calls to the column-major Fortran
  ABI. Getting a flip wrong yields a transposed or mirror-triangle result
  silently, with no crash. `src/c/netlib/test_netlib_cblas.c` compares the
  shim against naive reference implementations for exactly this reason, and
  runs as part of `make check` for this backend — every mapping it covers
  has been confirmed to fail when deliberately broken, so treat a failure
  there as real. Add a case for any routine you add to the shim; a mapping
  with no test is a mapping nobody has checked.
- A complex matrix asked for row-major with its conjugate transpose
  (`--trans C` on gemv, trmv and trsv) has no Fortran flag once flipped:
  the buffer read column-major is `A^T`, and `A^H` is its conjugate. The
  shim conjugates the vectors around a call with `N`, as reference CBLAS
  does, but in place rather than on a copy, so that the timed call
  allocates nothing (#228). Level 3 needs none of it: swapping the
  operands keeps each one's flag (the comment above `cblas_cgemm`).
- The shim declares the hidden `CHARACTER` length arguments the *GNU*
  Fortran ABI appends. A netlib built with ifort/ifx uses a different
  convention, so that combination is untested — if someone reports it, this
  is where to look. The same goes for a `REAL` function such as `sdot_`:
  gfortran returns a float, a library translated by f2c a double.
- A shared `libblas.so` carries its own Fortran runtime; a static
  `libblas.a` does not, and leaves `xerbla`'s `_gfortran_*` symbols
  dangling. `configure` tries the plain link first and retries with
  `-lgfortran`, so don't "simplify" that into a single unconditional
  `-lgfortran` — that would make a Fortran runtime a hard requirement for
  everyone.

**MKL** (#65) has a CBLAS but no `cblas.h`: it declares it in
`mkl_cblas.h`. `src/c/mkl/cblas.h` includes that, and goes on the include
path only for this backend, as `src/c/netlib` does (`BMB_CBLAS_CPPFLAGS`).

- Where it is: `/opt/intel/oneapi/mkl/latest` from Intel's repository,
  `$MKLROOT` (set by `setvars.sh`, modules and Spack, `lib` or, before
  2024, `lib/intel64`), `/usr/include/mkl` from Debian and Ubuntu's
  `libmkl-dev`. Both of the first two installed, Intel's wins.
- What is linked: `-lmkl_gf_lp64 -lmkl_gnu_thread -lmkl_core -lgomp`,
  Intel's link line for GCC with the GNU OpenMP layer and 32-bit integers.
  Not `-lmkl_rt`: by default it loads Intel's OpenMP runtime, a second one
  next to GCC's.
- Threads: `mkl_domain_set_num_threads(n, MKL_DOMAIN_BLAS)`, the highest
  of MKL's controls, so the count recorded is the one it was given;
  without `-t`, `MKL_NUM_THREADS` then `OMP_NUM_THREADS`, MKL's order. A
  `MKL_DOMAIN_NUM_THREADS` it overrides gets a warning. `MKL_DYNAMIC`
  (MKL may use fewer threads on a small problem) is left as MKL has it.
- The version is `mkl_get_version_string()`'s.

**AOCL-BLAS** (#66) is AMD's fork of BLIS, with BLIS's interface:
`cblas.h`, `bli_thread_set_num_threads()`, `bli_info_get_version_str()`
(which says `AOCL-BLAS 5.3.2 Build …`). It is a backend of its own
rather than BLIS found elsewhere, so that its results say `aocl` and
never merge with BLIS's in the report, and because what it links
differs:

- What is linked: `-lblis-mt`, its OpenMP build. Its `libblis` is the
  single-threaded one, which would ignore `-t`; it is taken only when
  there is no `libblis-mt`, with a warning. The checks for the thread
  and version calls run against whichever was linked
  (`AC_CHECK_FUNCS`), and define the same macros as BLIS, so
  `bmb_threads.c` and `bmb_build.c` need nothing of their own.
- Where it is: no distribution packages it. AMD's installer puts the
  headers in `include`, a build from source in `include/blis`;
  `configure` reads `AOCL_ROOT` (and `AOCL_INCDIR`, …) and Spack's
  `AMDBLIS_ROOT`, and adds `include/blis` under either when its
  `cblas.h` is there, with `-isystem`: its `cblas.h` has BLIS's unused
  static helpers.
- Threads: `BLIS_NUM_THREADS`, then `OMP_NUM_THREADS`, as BLIS.
  **AOCL Dynamic**, built in by default, may run a small problem on fewer
  threads than it is given, as `MKL_DYNAMIC` does; only a library built
  with `--disable-aocl-dynamic` turns it off.
- CI builds it from AMD's sources (`amd/blis`, a release pinned by
  commit) as AMD does: the `amdzen` configuration (every Zen generation,
  chosen at run time, and a generic path for other CPUs), OpenMP, CBLAS.
  The job checks that `libblis-mt` is linked and that 4 threads take
  clearly less time than 1.

**GPU libraries** (cuBLAS, rocBLAS, MAGMA, KBLAS, oneMKL's GPU side) are
planned for 2.0: #87 and its sub-issues. They do not fit the CBLAS layer
every backend above shares: a handle, operands in device memory, calls
that return before the work is done. This project once squeezed cuBLAS
and rocBLAS into it and backed them out; the GPU harness (#73), which
decides what is timed, comes before any of them.

**FlexiBLAS** (#5) is a backend that is not a library: it loads one at
run time (`FLEXIBLAS=OPENBLAS-OPENMP`, `BLIS-OPENMP`, `NETLIB`; `flexiblas
list` names them), so one build measures several. Three things follow:

- **The provenance is read at run time.** `bmb_build_backend()` returns
  `flexiblas/<the loaded library>` in lower case, and
  `bmb_build_blas_version()` FlexiBLAS's version and that library's name,
  from `flexiblas_current_backend()` and `flexiblas_get_version()`. The
  backend is part of the report's series key, so two runs of one binary
  under two libraries are two series, with no change to the report; a
  build-time name would have merged them, keeping the fastest of each
  point across libraries.
- **Threads** are set with `flexiblas_set_num_threads()`, which passes
  the count on. FlexiBLAS has no variable of its own, so the ones read
  without `-t` are those of the loaded library, chosen at run time by its
  name (`bmb_thread_env_vars()` in `bmb_threads.c`); none for Netlib.
  `test_bmb_threads_env.sh` reads the name from `--version` the same way.
- **Where to get it.** Fedora packages it as its default BLAS
  (`flexiblas-devel` and one package per library); Debian and Ubuntu do
  not, so its CI job runs in a Fedora container, once per library from
  the same build. Its headers live in a `flexiblas/` subdirectory;
  `FLEXIBLAS_ROOT`/`_INCDIR`/`_LIBDIR` work like the other backends'.
  Building FlexiBLAS itself needs a Fortran compiler and CMake.

**NVPL and ArmPL install from real, public, non-interactive apt repos** —
neither needs a login or EULA click-through. If either CI job starts
failing after an upstream version bump, don't assume it's permanently
broken; re-discover the current layout (see below) and fix the probe in
`configure.ac`:

- NVPL's `libnvpl-blas-dev` puts its CBLAS-compatible header at
  `/usr/include/nvpl_compat/cblas.h` (not the default include path) and
  links as `-lnvpl_blas_lp64_gomp`/`-lnvpl_blas_lp64_seq` (+
  `-lnvpl_blas_core`). The installer URL is version-pinned (no stable
  "latest" alias exists) — bump it in the CI job when
  [nvpl-downloads](https://developer.nvidia.com/nvpl-downloads) moves on.
- ArmPL's apt package (`arm-performance-libraries`) installs to a fixed
  `/opt/arm/arm-performance-libraries/{include,lib}` prefix. (The docs
  describe a different, version-numbered `/opt/arm/armpl_<version>_gcc/`
  layout used by the manual tarball installer — that's kept as a fallback
  probe, but the apt package doesn't use it.)
- To re-discover either layout: add a throwaway debug step to the CI job
  (`find /opt/arm`, or `dpkg -L <package> | grep cblas.h`) and read the
  log — don't trust vendor docs over the actual installed files.

## Finding a BLAS that isn't in /usr

Clusters expose BLAS through modules, not distro packages, so `configure`
probes, for the selected backend (plus generic `BLAS_*`/`CBLAS_*`):

- the install prefix, as `<PKG>_ROOT`, `<PKG>_DIR` or `<PKG>_HOME` — three
  names because module files are not consistent about it, and a benchmark
  that cannot find the library it was asked for is no use;
- the directories directly, as `<PKG>_INCDIR`/`<PKG>_INC` and
  `<PKG>_LIBDIR`/`<PKG>_LIB`.

That lives in the `BMB_ENV_HINTS`/`BMB_ENV_PREFIX_HINT`/`BMB_ADD_INCDIR`/
`BMB_ADD_LIBDIR` macros at the top of `configure.ac`.
`--with-blas-incpath`/`--with-blas-libpath` do the same thing explicitly,
and win over both (`BMB_USER_PATHS`, below).

Two things to preserve when touching that code:

- Paths are *prepended*, so whatever is added last wins. In each backend
  the order is: the hardcoded distro probe, then the module's variables,
  then `BMB_USER_PATHS` (`--with-blas-incpath`/`--with-blas-libpath`),
  so a loaded module beats a system-wide install, and the command line
  beats both. The options used to be applied as they were read, before
  the probes, and a library named on the command line linked the
  system's instead (#58); `tests/test_configure_paths.sh` checks the
  order. Within `BMB_ENV_HINTS` the prefixes are applied least-specific
  first, so `_ROOT` beats `_DIR` beats `_HOME`.
- Anything derived from a prefix variable must be guarded on that variable
  being non-empty — which is what `BMB_ENV_PREFIX_HINT` is for.
  `"$FOO_ROOT/lib"` with `FOO_ROOT` unset collapses to `/lib`, which
  exists — and would silently land in `-L`/`-rpath`.

When touching `configure.ac`'s backend-selection logic, don't let checks for
one backend leak into another: e.g. `AC_CHECK_LIB([openblas],
[openblas_set_num_threads])` must only run when OpenBLAS is actually the
library that got linked (see the `auto` case, which branches on
`$ac_cv_search_cblas_dgemm` for exactly this reason) — otherwise a system
that happens to have multiple BLAS libraries installed will `AC_DEFINE` a
`HAVE_*_SET_NUM_THREADS` macro for a library that isn't actually in `LIBS`,
and the link will fail with an undefined reference.

If you add another backend, do it fully — `configure.ac` detection, the
thread-count API in `bmb_threads.c` if it has one, a CI job, and its
Spack provider in `BACKENDS` ([packaging](packaging.md)) — rather
than a partial `--with-blas-backend=X` case that fails at compile or link
time. Leaving a backend entirely unimplemented is fine; leaving it
half-done is not.
