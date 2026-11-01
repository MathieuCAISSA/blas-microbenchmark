---
name: blas-compare
description: Compare BLAS libraries (OpenBLAS, BLIS, MKL, AOCL-BLAS, ...), library versions, or settings on one machine with blas-microbenchmark, and say which differences are real. Use when asked whether one BLAS is faster than another, whether an upgrade changed performance, or to compare two configurations.
---

# Compare BLAS libraries

A comparison is the same measurement, on the same machine, under the same
conditions, for each library: only the library may change. Use the
`bmb_mcp` tools when you have them (`run_suite`, `make_report`); otherwise
the commands below.

## 1. One build a library

blas-microbenchmark is built against one library at a time. Either:

- one install (or build tree) a library, e.g. from Spack:
  `spack install blas-microbenchmark ^openblas`, `... ^blis`;
- or one build against FlexiBLAS, which loads the library `FLEXIBLAS`
  names (`flexiblas list` gives them).

## 2. Run the same suite for each

```bash
bmb_suite --blas openblas=/opt/bmb-openblas --blas blis=/opt/bmb-blis --profile quick -o results
```

or, with one build against FlexiBLAS:

```bash
bmb_suite --blas flexiblas:OPENBLAS-OPENMP,BLIS-OPENMP --profile quick -o results
```

or `run_suite` with `blas` (a name and a suite each) or `flexiblas`. Each
library's results go into a directory of their own, each checked to come
from the library it names. Give every library the same options: threads,
pinning, cache state. `--routine` or `--level` narrows the suite.

## 3. Repeat the runs

A single run shows differences but not which ones are noise. Run the
whole comparison several times, each into its own directory, e.g.
`results/run1`, `results/run2`, `results/run3`.

## 4. Make the report

```bash
bmb_report results > report.html
```

or `make_report`. The report draws each library against a reference
(OpenBLAS when there is one), and with repeated runs tests each
difference against their spread: a hollow point is within the noise.

## 5. Say what the comparison shows

- A difference counts only when it is beyond the noise of the repeated
  runs; say how many runs it rests on.
- Name the sizes and thread counts: a library faster at one size can be
  slower at another, or at another thread count.
- Results from different machines, or measured under a different
  frequency setting, are not a comparison of libraries.
- Name the libraries by their version and the kernels they chose
  (`--version`), not only by name.
