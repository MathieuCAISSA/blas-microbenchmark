# Security

## Supported versions

Fixes go into the latest release. Use the newest version from the
[releases page](https://github.com/MathieuCAISSA/blas-microbenchmark/releases/latest).

## Reporting a vulnerability

Please do not open a public issue. Report it privately through GitHub:
**Security** tab of the repository, then **Report a vulnerability**
([direct link](https://github.com/MathieuCAISSA/blas-microbenchmark/security/advisories/new)).

Say what is affected, how to reproduce it, and which version. You will
get an answer within a week; a fix, and an advisory crediting you if you
wish, follow in a release.

## What counts

The benchmarks run locally on inputs the user gives them, so the useful
reports are about input that someone else could supply:

- a result file that makes `bmb_report` produce a page which runs script
  or loads anything from the network (the page is meant to be
  self-contained, with every value inserted as text);
- command-line input that makes a benchmark read or write out of bounds,
  or allocate far more than it reports (sizes are checked before any
  allocation).

A benchmark being slow, or a BLAS library misbehaving, is not a security
problem: please open a normal issue.
