# Backends

How each BLAS library is found, linked and named. Start at
[AGENTS.md](../../AGENTS.md).

| Backend | `configure` flag | Verified |
| --- | --- | --- |
| OpenBLAS | `--with-blas-backend=openblas` (default via `auto`) | CI, `openblas` job |
| BLIS | `--with-blas-backend=blis` | CI, `blis` job |
| Netlib | `--with-blas-backend=netlib` | CI, `netlib` job |
| NVPL | `--with-blas-backend=nvpl` | CI, `nvpl` job (`ubuntu-24.04-arm`) |
| ArmPL | `--with-blas-backend=armpl` | CI, `armpl` job (`ubuntu-24.04-arm`) |
| cuBLAS / rocBLAS | — | Not implemented, not planned |

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
where it does not.

The machine side of the provenance lives in `bmb_machine.c`, probed once per
run: CPU model, logical CPUs, NUMA nodes, cpu0's caches, the CPU frequency
governor and turbo, `uname`, and the date. Three rules there:

- **Every field is best effort and omitted when unreadable** — never
  guessed, never written empty. A container without sysfs cache entries is
  a legitimate "unknown", which is why `test_bmb_machine` checks only what
  every Linux box guarantees plus the sanity of whatever was recorded.
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

**`BMB_MACHINE_ROOT=DIR`** makes `bmb_machine()` probe `DIR/proc` and
`DIR/sys` instead of the real ones, as `test_bmb_machine` does through
`bmb_machine_probe_at()`. It is a test hook, so that the output and the
warning can be checked against the fixtures in
`src/c/common/fixtures/machine/` (`test_bmb_output_formats.sh`, and the
report's render test); it is not in the man page. The logical CPU count,
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
  `ldd src/c/level3/bmb_dgemm` actually resolves to.
- The shim translates row-major CBLAS calls to the column-major Fortran
  ABI. Getting a flip wrong yields a transposed or mirror-triangle result
  silently, with no crash. `src/c/netlib/test_netlib_cblas.c` compares the
  shim against naive reference implementations for exactly this reason, and
  runs as part of `make check` for this backend — every mapping it covers
  has been confirmed to fail when deliberately broken, so treat a failure
  there as real. Add a case for any routine you add to the shim; a mapping
  with no test is a mapping nobody has checked.
- The shim declares the hidden `CHARACTER` length arguments the *GNU*
  Fortran ABI appends. A netlib built with ifort/ifx uses a different
  convention, so that combination is untested — if someone reports it, this
  is where to look.
- A shared `libblas.so` carries its own Fortran runtime; a static
  `libblas.a` does not, and leaves `xerbla`'s `_gfortran_*` symbols
  dangling. `configure` tries the plain link first and retries with
  `-lgfortran`, so don't "simplify" that into a single unconditional
  `-lgfortran` — that would make a Fortran runtime a hard requirement for
  everyone.

**cuBLAS / rocBLAS**: deliberately out of scope. Handle-based,
device-memory API, fundamentally different from the CBLAS interface every
other backend shares — this project tried it once and backed it out; don't
re-add it without being asked.

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
`--with-blas-incpath`/`--with-blas-libpath` do the same thing explicitly.

Two things to preserve when touching that code:

- Paths are *prepended*, so whatever is added last wins. Backend-specific
  hints are applied after the hardcoded distro probes on purpose, so a
  loaded module beats a system-wide install; and within `BMB_ENV_HINTS`
  the prefixes are applied least-specific first, so `_ROOT` beats `_DIR`
  beats `_HOME`.
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
thread-count API in `bmb_threads.c` if it has one, and a CI job — rather
than a partial `--with-blas-backend=X` case that fails at compile or link
time. Leaving a backend entirely unimplemented is fine; leaving it
half-done is not.
