# CI

What each CI job is for. Start at [AGENTS.md](../../AGENTS.md).

`ci.yml` gives its jobs a read-only token (`permissions: contents:
read`); only `pages.yml` and `release.yml` ask for more, for what they
publish. Every action is pinned to a full commit SHA, with its version
in a comment (`actions/checkout@3d3c42e… # v7.0.1`): a tag can be moved to
other code, a commit cannot. `test_actions_pinned.sh` refuses anything
else. Dependabot (`.github/dependabot.yml`) proposes updates of the
actions, grouped, once a week, and updates the SHA and the comment
together; to pin a new action by hand, take the commit of its latest
release (`gh api repos/OWNER/REPO/commits/vX.Y.Z -q .sha`). `main` is protected: no force push or
deletion, and a pull request needs every CI job green to merge.

GitHub Actions (`.github/workflows/ci.yml`) runs `./autogen.sh`,
`configure --enable-werror`, `make`, and `make check` on every push/PR,
once per implemented backend: `openblas`, `blis` and `netlib` on
`ubuntu-latest`, `nvpl` and `armpl` on `ubuntu-24.04-arm` (GitHub's free
Linux arm64 hosted runner for public repos — both those backends are
aarch64-only). Keep it green — a routine that only works "on my machine"
isn't done.

The `openblas` job also installs into a staging root and fails if anything
lands outside `--prefix`, or if `bin/bmb_report`, the `libexec`
benchmarks and the man pages are not where the README says; and it fails
if the browser tests skipped, since the x86 runners have Chrome.

The comment above each job is published on the site's Development page,
with its runner, configure line and steps: keep it saying why the job
exists.

The `browsers` job builds with OpenBLAS and runs only `src/report`'s tests,
once in Firefox (through geckodriver) and once in Edge, both preinstalled
on the runner image — Edge is installed from Microsoft's repository if it
ever stops being. It exists because Firefox is the browser users are told
to use, and Chrome passing says nothing about it.

The `man` job runs the man page tests in a Fedora container: another
groff and man-db build than Ubuntu's, plus mandoc for its lint, with
`BMB_MAN_STRICT=1` so that a missing tool fails instead of skipping. It
builds without `--enable-werror`, since Fedora's newer GCC is not what it
is there to test; Fedora keeps `cblas.h` in `/usr/include/openblas`, which
it finds through `OPENBLAS_INCDIR`, the hint a cluster module would set.

The site is published by its own workflow, `pages.yml` (see [The
site](documentation.md#the-site)), not by `ci.yml`.

Every job that builds the benchmarks also runs `.github/verify-sweep.sh`
("Verified at real sizes"): all 20, checked at sizes a user measures — a
few MB per operand — with non-square shapes and 1, 2 and 4 threads, on
that job's library. `make check` runs `--verify` on tiny sizes only, and
a tolerance too tight for a real size, or a library that is only wrong
once it splits the work between threads, would get through it. It takes
seconds; the sanitizers job runs it under ASan and UBSan too.

The `sanitizers` job rebuilds at `-O1` under ASan and UBSan. It is
not a duplicate of the `openblas` job: it sees what a plain build cannot
(out-of-bounds accesses, signed overflow), and the different optimisation
level moves GCC's diagnostics around — the first `-Werror` failure it ever
produced was a format-truncation warning invisible at `-O2`.

Leak detection is on, with the BLAS library's own per-thread buffers --
which it never frees by design -- suppressed by module in
`.github/lsan-suppressions.txt`. That keeps it useful for our code: an
option string that an early return forgot to free is exactly the kind of
thing it catches. To reproduce locally:

```bash
../configure --enable-werror CFLAGS="-O1 -g -fsanitize=address,undefined" \
    LDFLAGS="-fsanitize=address,undefined"
ASAN_OPTIONS=detect_leaks=1 \
  LSAN_OPTIONS=suppressions=$PWD/../.github/lsan-suppressions.txt make check
```
