# Contributing

Thank you for helping. This file is the short version; the developer guide
is [AGENTS.md](AGENTS.md), written for people and coding agents alike,
and the files in `doc/dev/` it points to.

## Reporting a problem

[Open an issue](https://github.com/MathieuCAISSA/blas-microbenchmark/issues/new/choose).
For a bug or a number that looks wrong, include what
`bmb_<routine> --version` prints: it names the version, the BLAS library
and the machine, which is most of what is needed to reproduce a result.

A security problem goes through [SECURITY.md](.github/SECURITY.md) instead, not a
public issue.

Everyone taking part follows the [code of conduct](.github/CODE_OF_CONDUCT.md)
(the Contributor Covenant 3.0). It says how to report a problem with
someone's behaviour, publicly or not.

## Building and testing from git

```bash
./autogen.sh                 # needs autoconf, automake, libtool
mkdir build && cd build
../configure
make
make check
```

Some tests need tools and skip without them: the report's tests a browser
(Chrome, Chromium, Edge, or Firefox with geckodriver), the man page tests
groff and `man`, the site's tests mandoc. CI runs them all. The
[Development page](https://mathieucaissa.github.io/blas-microbenchmark/development.html)
lists every test and every CI job.

## Working on a change

Every change has an issue and a branch of its own, named after it
(`issue-<N>-<short-name>`), and reaches `main` through a pull request whose
description says `Closes #<N>`. `main` only accepts a pull request once
every required CI job passes (all but `coverage`). The details are in
[AGENTS.md, Working on an issue](AGENTS.md#working-on-an-issue).

## Before opening a pull request

Go through [REVIEWS.md](.github/REVIEWS.md): it is the grid every pull request is
reviewed against, and the review is posted on the pull request while CI
runs. The essentials:

- `make check` passes. It holds every file to
  [.editorconfig](.editorconfig), which most editors follow, natively or
  with a plugin. `make distcheck` too if you touched `configure.ac`, a
  `Makefile.am`, or added or removed a file.
- A new test has been seen to fail: break the code it guards, watch it
  fail, restore it ([AGENTS.md, Tests](AGENTS.md#tests)).
- A new option, output field or behaviour is in the man page, with the
  same default as in `--help` — the tests check it
  ([doc/dev/documentation.md](doc/dev/documentation.md)).
- The change is in the `[Unreleased]` section of
  [CHANGELOG.md](CHANGELOG.md) if a user would notice it.

Every pull request runs the full CI; it has to pass before merging.

Adding a BLAS routine or a backend has its own checklist:
[a routine](doc/dev/benchmarks.md#adding-a-new-blas-routine-benchmark),
[a backend](doc/dev/backends.md).

## License

Contributions are made under the project's license,
[Apache 2.0](LICENSE).
