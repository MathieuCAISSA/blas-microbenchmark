# CI

What each CI job is for. Start at [AGENTS.md](../../AGENTS.md).

`ci.yml` gives its jobs a read-only token (`permissions: contents:
read`); only `pages.yml`, `release.yml` and `codeql.yml` ask for more:
the first two for what they publish, CodeQL to upload its results. Every action is pinned to a full commit SHA, with its version
in a comment (`actions/checkout@3d3c42e… # v7.0.1`): a tag can be moved to
other code, a commit cannot. `test_actions_pinned.sh` refuses anything
else. Dependabot (`.github/dependabot.yml`) proposes updates of the
actions, grouped, once a week, and updates the SHA and the comment
together; to pin a new action by hand, take the commit of its latest
release (`gh api repos/OWNER/REPO/commits/vX.Y.Z -q .sha`). `main` is protected: no force push or
deletion, and a pull request needs every required check green to merge:
every job of `ci.yml` except `coverage`, which is there to be read. A
new job goes into the required checks (the branch protection settings)
when it merges, or it can fail without stopping anything.

GitHub Actions (`.github/workflows/ci.yml`) runs `./autogen.sh`,
`configure --enable-werror`, `make`, and `make check` on every pull
request and push to `main`,
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

The six jobs that test a library (`openblas`, `blis`, `netlib`, `nvpl`,
`armpl`, `sanitizers`) also run `.github/verify-sweep.sh` ("Verified at
real sizes"): all 20, checked at sizes a user measures — a
few MB per operand — with non-square shapes, 1, 2 and 4 threads and both
storage orders, on
that job's library. `make check` runs `--verify` on tiny sizes only, and
a tolerance too tight for a real size, or a library that is only wrong
once it splits the work between threads, would get through it. It takes
seconds; the sanitizers job runs it under ASan and UBSan too.

## Coverage

The `coverage` job builds at `-O0` with `--coverage`, runs `make check`,
and reports with `gcovr` which lines and branches of the C code the tests
reached, the tests' own files left out. The per-file summary is on the
run's page; the line-by-line HTML report and an lcov file are in the
`coverage` artifact. AGENTS.md gives the same commands for a local run.

There is no threshold, on purpose: a number to keep above invites tests
written for the number. The report is for finding paths no test takes.
When it was added (1.3.0-dev) it read 90% of lines, 77% of branches; the
gaps were the out-of-memory paths of every benchmark, the option
parser's error messages, and `bmb_threads.c` at 38%: no test set a
thread-count environment variable (`OPENBLAS_NUM_THREADS`, ...).
`test_bmb_threads_env.sh` (#24) brought it to 100%, and found on the way
that `OMP_NUM_THREADS=-2` labelled a run with 4294967294 threads.

## Static analysis

Two analysers read the code rather than run it, so they see the paths no
test takes:

- The `cppcheck` job runs `.github/cppcheck.sh`, which fails on any
  finding. A wrong finding is suppressed on the line above it with
  `/* cppcheck-suppress <id> */` and a comment saying why;
  `variableScope` (narrowing each variable to its innermost block is a
  style this code does not follow, not a defect) and
  `missingIncludeSystem` are off everywhere, with the
  reason in the script. Its first run found pointer arithmetic on a
  `malloc` result before the NULL check in every `verify()`, undefined
  behaviour on the out-of-memory path, and bare `NULL` as the sentinel of
  a variadic call in the option tests. To run it locally, after
  configure: `.github/cppcheck.sh build` (`apt install cppcheck`, or
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
  reads, for the user running it, files that user can read already); the
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
`.github/lsan-suppressions.txt`. That keeps it useful for our code: an
option string that an early return forgot to free is exactly the kind of
thing it catches. To reproduce locally:

```bash
../configure --enable-werror CFLAGS="-O1 -g -fsanitize=address,undefined" \
    LDFLAGS="-fsanitize=address,undefined"
ASAN_OPTIONS=detect_leaks=1 \
  LSAN_OPTIONS=suppressions=$PWD/../.github/lsan-suppressions.txt make check
```
