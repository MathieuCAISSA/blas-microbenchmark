# CI

What each CI job is for. Start at [AGENTS.md](../../AGENTS.md).

`ci.yml` gives its jobs a read-only token (`permissions: contents:
read`), except the `coverage` job's OIDC token (`id-token: write`), which
authenticates its upload to Codecov; only `pages.yml`, `release.yml`,
`codeql.yml` and `scorecard.yml` ask for more: the first two for what
they publish, CodeQL to upload its results, Scorecard to sign the
results it publishes. Every action is pinned to a full commit SHA, with its version
in a comment (`actions/checkout@3d3c42e… # v7.0.1`): a tag can be moved to
other code, a commit cannot. `test_actions_pinned.sh` refuses anything
else. Dependabot (`.github/dependabot.yml`) proposes updates of the
actions, grouped, once a week, and updates the SHA and the comment
together; it does the same for the Docker image's base, pinned by digest
([packaging](packaging.md#the-docker-image)), and for the image
ClusterFuzzLite builds in ([fuzzing](#fuzzing)); to pin a new action by hand, take the commit of its latest
release (`gh api repos/OWNER/REPO/commits/vX.Y.Z -q .sha`). `main` is protected: no force push or
deletion, and a pull request needs every required check green to merge:
every job of `ci.yml` except `coverage`, which is there to be read. A
new job goes into the required checks (the branch protection settings)
when it merges, or it can fail without stopping anything.

GitHub Actions (`.github/workflows/ci.yml`) runs `./autogen.sh`,
`configure --enable-werror`, `make`, and `make check` on every pull
request and push to `main`,
once per implemented backend: `openblas`, `blis`, `aocl` (AOCL-BLAS,
built from AMD's sources), `netlib` and `mkl` on `ubuntu-latest`,
`flexiblas` in a Fedora container, `nvpl` and `armpl` on
`ubuntu-24.04-arm` (GitHub's free
Linux arm64 hosted runner for public repos — both those backends are
aarch64-only). Keep it green — a routine that only works "on my machine"
isn't done.

The `openblas` job also installs into a staging root and fails if anything
lands outside `--prefix`, or if `bin/bmb_report`, `bin/bmb_suite`, the
`libexec` benchmarks and the man pages are not where the README says;
installed for real, `bmb_suite` runs two of the benchmarks there; and it fails
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

The `spack` job builds the Spack package with Spack, against OpenBLAS,
BLIS, AOCL-BLAS, Netlib, FlexiBLAS and MKL, from this commit's tarball and from the
latest release, and runs its tests (`spack test run`), through
`.github/scripts/spack-check.sh`; see [packaging](packaging.md#in-ci).

The `docker` job builds the Docker image once per backend it offers
(OpenBLAS, BLIS, Netlib, MKL) and runs every benchmark from it, then
what a user of the image relies on: `man`, results in a mounted
directory, the warning under a CPU quota, and, once, hadolint and
Apptainer; see [packaging](packaging.md#in-ci-1).

The `reframe` job runs the ReFrame tests on an install and on this
commit's tarball, with references met and not, and checks their list of
routines against the benchmarks; see [packaging](packaging.md#in-ci-2).

The site is published by its own workflow, `pages.yml` (see [The
site](documentation.md#the-site)), not by `ci.yml`.

The nine jobs that test a library (`openblas`, `blis`, `aocl`, `netlib`,
`nvpl`, `armpl`, `flexiblas`, `mkl`, `sanitizers`) also run `.github/scripts/verify-sweep.sh` ("Verified at
real sizes"): all 32, checked at sizes a user measures — a
few MB per operand — with non-square shapes, 1, 2 and 4 threads and both
storage orders, on
that job's library. `make check` runs `--verify` on tiny sizes only, and
a tolerance too tight for a real size, or a library that is only wrong
once it splits the work between threads, would get through it. It takes
seconds; the sanitizers job runs it under ASan and UBSan too.

## Coverage

The `coverage` job builds at `-O0` with `--coverage`, runs `make check`,
and reports with `gcovr` which lines and branches of the C code the tests
reached, the tests' own files left out (`test_*`, and the fuzz targets'
`fuzz_*`). It does so once per backend Ubuntu packages, OpenBLAS, Netlib
and BLIS: code that only runs under one of them (a library with no
version string, a single-threaded one, Netlib's CBLAS layer) counts
where it runs, and lines inside another backend's `#if` are not compiled,
so not counted (#132). The per-file summary is on the run's page; the
line-by-line HTML report, an lcov file and a Cobertura one are in the
`coverage-<backend>` artifact. AGENTS.md gives the same commands for a local run.
The Cobertura file goes to [Codecov](https://codecov.io/gh/MathieuCAISSA/blas-microbenchmark),
which draws the README's coverage badge and the history of the figure,
merging the three reports of a commit (each flagged with its backend):
uploaded with the job's OIDC token, so no secret is stored, and without
failing the job when Codecov is down. `.github/codecov.yml` keeps its
statuses informational and its pull request comments off. Codecov's
figure was lower than gcovr's for the same report (87% against 92.5% of
lines, at 1.5.0): it counts a line where only some branches were taken
as partial, not covered.

The project's answers to the [OpenSSF Best Practices](https://www.bestpractices.dev/projects/15515)
criteria, which the README's badge shows (passing, at 1.5.0), are kept
on bestpractices.dev; each points at the file here that meets it, so a
change that moves one of those files (`CONTRIBUTING.md`,
`.github/SECURITY.md`, `REVIEWS.md`) updates the answers there.

There is no threshold, on purpose: a number to keep above invites tests
written for the number. The report is for finding paths no test takes.
When it was added (1.3.0-dev) it read 90% of lines, 77% of branches; the
gaps were the out-of-memory paths of every benchmark, the option
parser's error messages, and `bmb_threads.c` at 38%: no test set a
thread-count environment variable (`OPENBLAS_NUM_THREADS`, ...).
`test_bmb_threads_env.sh` (#24) brought it to 100%, and found on the way
that `OMP_NUM_THREADS=-2` labelled a run with 4294967294 threads. The
out-of-memory paths were the rest: `test_bmb_alloc_failures.sh` (#111)
makes each of the 200 or so allocations of the 32 benchmarks fail in
turn, and took lines from 92.5% to 98.3% and branches from 81% to 91%
(Codecov's figure, which counts partly taken branches against a line,
moves with the second).

#131 to #134 took Codecov's figure from 95% to 100%: a coverage job per
backend Ubuntu packages (#132), dead code removed (#133), tests for every
path a test could take (#131, #134, #146), which found three
descriptions cut short. What is left cannot run in the coverage jobs'
builds (OpenBLAS, Netlib and BLIS, on glibc Linux), and is marked where it
is with gcovr's `GCOVR_EXCL_LINE`, `GCOVR_EXCL_BR_LINE` or
`GCOVR_EXCL_START`/`STOP`, and the reason after a colon, such as:
`uname()` or `clock_gettime(CLOCK_MONOTONIC)` failing, a call shorter
than the clock resolves, `snprintf` returning a negative count, a sweep
of 257 points, which is longer than any may be, glibc's `getopt` leaving
`optind` past `argc`, a branch only FlexiBLAS with Netlib loaded takes
(the `flexiblas` job runs it). An exclusion is for a line no test can
reach in those builds, never for one nobody wrote a test for: the review checks the
reason, and `test_coverage_exclusions.sh` that there is one. Codecov is
sent gcovr's Cobertura report, not its lcov one, which in gcovr 7.0 (the
runners') keeps excluded lines as never run.

## OpenSSF Scorecard

`scorecard.yml` runs [OpenSSF Scorecard](https://scorecard.dev/viewer/?uri=github.com/MathieuCAISSA/blas-microbenchmark)
on every push to `main` and weekly: it scores the project's security
practices (pinned actions, token permissions, branch protection, code
review, signed releases, ...) and publishes the result, which the
README's badge shows. Its job is limited to what publishing allows:
checkout, the Scorecard action, the upload of its SARIF as an artifact.
The SARIF is not uploaded to code scanning, where the CodeQL workflow
fails on any open alert: Scorecard's findings are a score to read, not
defects to dismiss one by one. The `Branch-Protection` check needs a
token that can read the branch protection settings, which the job's own
cannot; without one, Scorecard leaves the check out of the score. It is
left without one on purpose: the higher tiers need pull requests
approved by someone other than their author, which a project with one
maintainer cannot have, so the check would score low and pull the
score down. `Signed-Releases` averages the last five releases: 8 for
the attestation bundle `release.yml` attaches (`<tarball>.sigstore.json`,
#107), 10 for the provenance beside it (`<tarball>.intoto.jsonl`, #127).
1.3.0 to 1.5.0 were attested when built, and their bundle and provenance
were attached afterwards, from `gh attestation download`; 1.2.0 and
older have none. `Packaging` counts the push of the Docker images to
ghcr.io (#108). `Maintained` is 0 for a repository less than 90 days
old, until 2026-12-20 here.

## Static analysis

Two analysers read the code rather than run it, so they see the paths no
test takes:

- The `cppcheck` job runs `.github/scripts/cppcheck.sh`, which fails on any
  finding. A wrong finding is suppressed on the line above it with
  `/* cppcheck-suppress <id> */` and a comment saying why;
  `variableScope` (narrowing each variable to its innermost block is a
  style this code does not follow, not a defect) and
  `missingIncludeSystem` are off everywhere, with the
  reason in the script. Its first run found pointer arithmetic on a
  `malloc` result before the NULL check in every `verify()`, undefined
  behaviour on the out-of-memory path, and bare `NULL` as the sentinel of
  a variadic call in the option tests. A family's source
  (`src/c/level<N>/bmb_<family>.c`) is read once per precision
  `src/c/routines.txt` gives it, with that precision's `-DBMB_PREC_<P>`:
  cppcheck, given a `-D`, reads a file in that configuration alone, and
  with no precision `bmb_prec.h` stops at its `#error`, so a run over the
  tree would leave the family unread (#200). To run it locally, after
  configure: `.github/scripts/cppcheck.sh build` (`apt install cppcheck`, or
  `pip install cppcheck`).
- CodeQL (`codeql.yml`, its own workflow) builds the code and runs
  GitHub's security and quality queries, which follow data across
  functions. It runs on every pull request and push to `main`, and
  weekly, since the queries change on their own. Its findings go to the
  Security tab (code scanning), where each is fixed, or dismissed with a
  reason; a pull request's `CodeQL` check fails on a new one in its
  diff, and the run on `main` fails while any is open (below).

  **A pull request's analysis only reports on the lines it changes**
  (its `CodeQL` check fails on a new alert there); the full picture is
  the analysis of `main`. #12's pull request showed no result while
  `main` had four. So outside pull requests (on `main`, weekly, by
  hand), the workflow's last step fails while an alert is open on the
  branch it analysed, and lists them. To list them yourself:

  ```bash
  gh api 'repos/MathieuCAISSA/blas-microbenchmark/code-scanning/alerts?state=open' \
    -q '.[] | "\(.number) \(.rule.id) \(.most_recent_instance.location.path):\(.most_recent_instance.location.start_line)"'
  ```

  Dismissed so far, each with its reason in the alert: the path
  injections from the `BMB_MACHINE_ROOT` test hook (used in tests: it
  reads, for the user running it, files that user can read already), and
  the same for the seed files `fuzz_bmb_options` replays; the
  `-o` file created under the user's umask (won't fix); the exact
  comparison of exactly representable values in `test_bmb_verify.c`;
  the length of `bmb_options_parse()` without comments (the code style
  is comments for *why* only).

## Sanitizers

The `sanitizers` job rebuilds at `-O1` under ASan and UBSan. It is
not a duplicate of the `openblas` job: it sees what a plain build cannot
(out-of-bounds accesses, signed overflow), and the different optimisation
level moves GCC's diagnostics around — the first `-Werror` failure it ever
produced was a format-truncation warning invisible at `-O2`.

Leak detection is on, with the BLAS library's own per-thread buffers --
which it never frees by design -- suppressed by module in
`.github/scripts/lsan-suppressions.txt`. That keeps it useful for our code: an
option string that an early return forgot to free is exactly the kind of
thing it catches. To reproduce locally:

```bash
../configure --enable-werror CFLAGS="-O1 -g -fsanitize=address,undefined" \
    LDFLAGS="-fsanitize=address,undefined"
ASAN_OPTIONS=detect_leaks=1 \
  LSAN_OPTIONS=suppressions=$PWD/../.github/scripts/lsan-suppressions.txt make check
```

## Fuzzing

The `fuzz` job runs on pull requests only: [ClusterFuzzLite](https://google.github.io/clusterfuzzlite/)
builds the fuzz targets with clang and libFuzzer in OSS-Fuzz's builder
image (`.clusterfuzzlite/Dockerfile`, pinned by digest, which Dependabot
moves; `build.sh`), then fuzzes each for five minutes, once under ASan and
once under UBSan. There is one target, `fuzz_bmb_options`
(`src/c/common`): the command-line parser, where everything a user types
arrives. Each input is an argv, split on its NUL bytes; whatever the
parser accepts is held to what the benchmarks rely on (the checks are
in the target), so a wrong answer fails as surely as a crash. Its seed
corpus (`fuzz/bmb_options/`) is the man page's examples and the edges the
unit tests name, its dictionary the options and separators.

A failure in the job names the input that caused it, in the job's
artifacts: add it to the corpus with the fix, so `make check`, which
replays the corpus through the same function in every job, keeps it
fixed. To replay one input locally: `src/c/common/fuzz_bmb_options FILE`
from a build directory, ideally a sanitizer build (above).

ClusterFuzzLite keeps no corpus between runs here (that needs a storage
repository): each run starts from the seeds. OpenSSF Scorecard's
`Fuzzing` check looks for `.clusterfuzzlite/`.
