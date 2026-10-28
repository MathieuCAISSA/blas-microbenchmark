# Reviewing a change

The one grid for reviewing a pull request here. It covers what CI
enforces, so that the author catches it first, and what no test sees.
The author goes through it before asking for a merge; the reviewer uses
it, and only it, and posts the review on the pull request while CI runs:
each section, *ok* or what is wrong, with the file and line, looked up
in the file rather than remembered (the first review here cited two
lines wrong, #26).

Each item is one line, with a link to the rule it comes from. If an item
and the rule behind it disagree, the rule wins: fix the item.

**This list grows.** A bug that a review should have caught, but did
not, adds an item here, in the pull request that fixes it, with the bug
it comes from in brackets.

## The change

- [ ] It does what its issue asks, and nothing else; anything more is
  another issue ([AGENTS.md](../AGENTS.md#working-on-an-issue)).
- [ ] Each commit builds and passes `make check`, and its message says
  why, not only what.

## Code quality

- [ ] Every line of the diff is where it was meant to be: an edit made
  by a script, on an anchor that occurs twice, changes both (#163 put
  the new defaults in `-k`'s case as well as in the defaults, #190).
- [ ] No new warning under `-Wall -Wextra`; `_POSIX_C_SOURCE` first in a
  file using POSIX functions ([code style](../AGENTS.md#code-style)).
- [ ] `common/` stays generic: shapes, alpha values and resets belong to
  the routine's file; no exported function without a caller.
- [ ] Comments say why, not what.
- [ ] No pointer arithmetic before the pointer is checked: offsets into
  a `malloc` result are computed after its NULL check (every `verify()`
  did it before, #12). A cppcheck finding is fixed, or suppressed where
  it is with the reason, and the author runs `.github/scripts/cppcheck.sh build`
  before pushing rather than leave it to CI (#3 did)
  ([static analysis](../doc/dev/ci.md#static-analysis)). A change to
  what an analyser reads is seen to still find a bug planted where it
  applies: suppressing `bmb_prec.h`'s `#error` silenced cppcheck, and
  left the GEMM family unread (#200).
- [ ] Every error is checked and reported: a failed allocation, write or
  `fclose` makes the run fail, never pass short (a new allocation is
  failed by `test_bmb_alloc_failures.sh` on its own)
  ([limitations](../doc/dev/benchmarks.md#known-measurement-limitations)).
- [ ] Shell: POSIX `sh`; every expansion quoted, unless word splitting
  is wanted and a comment or `# shellcheck disable=SC2086` says so; no
  pipe that hides a failure (write to a file, then check it)
  ([the site](../doc/dev/documentation.md#the-site)).
  A file name from the user reaches a command on its standard input or
  after `--`, never where a leading `-` makes it an option: `bmb_report
  -- -d.json` gave `-d.json` to `sed` (#282).
- [ ] awk: a function that calls `match()` resets `RSTART` and
  `RLENGTH` for its caller; copy them before calling it (the link
  checker checked links twice, #16).

## Tests

- [ ] New behaviour has a test, and the test was seen to fail with the
  code it guards broken, by a mutation that still compiles
  ([tests](../AGENTS.md#tests)).
- [ ] Tiny sizes only; nothing allocates more than a few MB.
- [ ] Current formats are generated, not kept as fixtures.
- [ ] A test that SKIPs where its tool is missing is required to pass
  in at least one CI job.
- [ ] A test works in a tree configured in place (`./configure` at the
  top, as the README and the release workflow do) as well as in a build
  directory of its own, as CI does (#98 stopped the 1.5.0 release).
- [ ] A check reads what it checks: a tool's real output, not its exit
  status alone (`gh attestation verify` prints nothing outside a
  terminal, #13), and only the part of a page that was rendered
  ([testing the page](../doc/dev/report.md#testing-the-page)).
- [ ] A test that times calls holds them only to what the clock
  guarantees: a lower bound on time, an upper bound on calls. It never
  counts on two samples differing: equal calls can tie to the clock's
  resolution, and then spread by nothing (#183). Nor on them staying
  alike, or few: one preempted sample on a loaded machine spreads them,
  and a slow one takes the time alone. A check that needs exact samples
  runs on a clock the test moves itself (`test_bmb_bench`'s
  `bmb_timer_now()`, #218).
- [ ] A test looks for the message it expects, never for an empty
  stderr from a run on the real machine: one that exposes its CPU
  frequency adds a warning to every run (a CI runner did, #48).
- [ ] A test that edits or reads a result file does not count on an
  optional field: `blas` is absent under Netlib, NVPL and ArmPL (#9's
  render test rewrote a line that was not there).
- [ ] A coverage exclusion (`GCOVR_EXCL_*`) is for a line no test can
  reach in the coverage jobs' builds (OpenBLAS, Netlib, BLIS, on glibc
  Linux), and its reason says why; one nobody wrote a test for
  gets the test ([coverage](../doc/dev/ci.md#coverage)).
- [ ] A moved file is looked for under every form of its path, including
  one assembled from pieces (`os.path.join(..., f'level{level}', ...)`,
  `level$l`): a grep of the whole path missed ReFrame's, which the
  `reframe` job then skipped (#148).
- [ ] Text is not taken for binary: `grep -I` skips a file that is not
  UTF-8 (#15).
- [ ] A test of the Spack package is named for what it checks, never with
  a name Spack's own package class has: a method `test_suite` replaced
  Spack's attribute of that name, and every `spack test run` failed
  (#258). One of something new skips the releases before it
  (`self.spec.satisfies("@:1.8")`): the `spack` job tests the latest
  release's install too (#258 again).
- [ ] A change to what `bmb_options_parse()` accepts keeps the checks of
  `fuzz_bmb_options` true of everything it accepts, and a new form of
  option gets a seed in `src/c/common/fuzz/bmb_options/`, listed in
  `FUZZ_CORPUS` (`src/c/common/Makefile.am`) for the tarball
  ([fuzzing](../doc/dev/ci.md#fuzzing)).

## Security

- [ ] A number from outside (an option, an environment variable, a file)
  is parsed strictly: digits only, within bounds. `strtoul` takes `-2`
  and wraps it (4294967294 threads, #24).
- [ ] Sizes cannot wrap `size_t` or the `int` BLAS takes
  ([test_bmb_size_limits.sh](../AGENTS.md#tests)). The parser's ceiling
  is for elements of 8 bytes: a routine with larger ones checks its own
  products (zgemm's 16 wrapped at `-m 1073741824`, #6).
- [ ] A string from outside (a library, a configuration file, the
  user) that reaches the output is escaped in the JSON and kept off
  comment lines' line ends, or reduced to safe characters: FlexiBLAS's
  backend names come from configuration files (#5).
- [ ] Anything from a result file reaches the report's DOM through
  `textContent`, never `innerHTML`; the page loads nothing from
  elsewhere ([the page](../doc/dev/report.md#the-page)).
- [ ] An environment variable that reaches a file path has, next to its
  `getenv`, the reason it is safe (CodeQL flags it as path injection;
  `BMB_MACHINE_ROOT`, #3).
- [ ] After the merge, the CodeQL run on `main` is green: it fails while
  an alert is open there, which a pull request's analysis, covering only
  its diff, does not show (#12 showed none while `main` had four;
  [static analysis](../doc/dev/ci.md#static-analysis)).
- [ ] Before the merge, the pull request's CodeQL analysis has no alert,
  notes included: a note passes its check and fails the run on `main`
  (`gh api 'repos/MathieuCAISSA/blas-microbenchmark/code-scanning/alerts?ref=refs/pull/<N>/merge'`;
  #208 merged two `cpp/include-non-header`, #209). That list is not
  all `main` will see: #210's analysis had no result, and `main`'s
  found two `cpp/missing-header-guard` (#211). So no source includes a
  `.c` file, and a new header has an include guard.
- [ ] No two floating-point values are compared with `==` or `!=`, tests
  included: CodeQL's `cpp/equality-on-floats` is only a note on the pull
  request, and fails the run on `main` (#179). Compare within a
  tolerance; a value against an exact literal is fine.
- [ ] A new action is pinned to a commit SHA with its version; a
  workflow asks only for the permissions it needs
  ([CI](../doc/dev/ci.md)).
- [ ] Nothing personal in what is published: no hostname, no email.

## Validity of the measurements

What makes a benchmark wrong without making it fail.

- [ ] `call()` makes the one BLAS call and nothing else; copies and
  resets happen in `reset()`, outside the timed window
  ([adding a routine](../doc/dev/benchmarks.md#adding-a-new-blas-routine-benchmark)).
- [ ] An operand that drifts over calls (to infinity, to denormals) is
  reset often enough, and `reset_every_call` is set only when needed.
- [ ] Rates use the conventional BLAS counts; level 3 declares no GB/s
  ([which rate](../doc/dev/benchmarks.md#which-rate-a-routine-declares)).
  A new operation has its formula in `test_bmb_counts.sh`, written from
  the convention, not copied from its source (#205).
- [ ] `verify()` calls `bmb_verify_perturb()`, and its tolerance was not
  widened to make a failure go away
  ([checking results](../doc/dev/benchmarks.md#checking-results---verify)).
- [ ] A number shown to the user is the one that was used: thread count,
  sizes, backend, the CPU string (whose form never changes)
  ([backends](../doc/dev/backends.md)).

## Documentation

- [ ] A new option, output field, environment variable or exit status is
  in the man page, with the same default as `--help`
  ([documentation](../doc/dev/documentation.md)). The man tests read
  long-only options too (`--trans`, `--batch-count`): until #159 they saw
  only `-x, --name`, and #221's four went unchecked.
- [ ] A man page change passes `BMB_MAN_STRICT=1 make -C man check` with
  mandoc installed, as the `man` job runs it: mandoc's lint refuses a
  text line over 80 bytes, which groff renders without a word (#67's
  first push, checked without mandoc).
- [ ] The README stays short; the reference is the man page.
- [ ] `CHANGELOG.md` has an entry under `[Unreleased]` if a user would
  notice the change.
- [ ] A new backend, routine or CI job is in every list that names
  them, which [the release's check-up](../doc/dev/release.md#cutting-a-release)
  enumerates: the README, the man page, AGENTS.md, `doc/dev`, packaging
  (#66's first push missed a dozen, #94). A routine is a line of
  `src/c/routines.txt`, its lists written by `src/c/routines.sh`, never
  edited by hand (#199).
- [ ] A new test's first comment paragraph, and a new CI job's comment,
  say what they check: the site publishes them.
- [ ] Every file the change adds is in git and, if it belongs in the
  tarball, in `EXTRA_DIST`; `make distcheck` passes. `git status`
  after a commit shows nothing left behind (a glob that matched only an
  ignored file skipped a release commit, 1.2.0).
