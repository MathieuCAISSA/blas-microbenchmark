---
name: blas-benchmark
description: Measure the speed of a BLAS routine (dgemm, ddot, dtrsm, ...) on this machine with blas-microbenchmark, so that the number can be trusted. Use when asked how fast a BLAS library runs a routine, for a GFLOP/s or GB/s figure, or to check a library's performance at given sizes or thread counts.
---

# Measure a BLAS routine

blas-microbenchmark has one benchmark a routine, `bmb_<routine>`, checked
against a reference before it is timed. Use the `bmb_mcp` tools when you
have them (`describe_machine`, `list_routines`, `run_benchmark`); otherwise
run the commands below. The steps are the same.

## 1. Know the machine and the library

```bash
bmb_dgemm --version
```

or `describe_machine`. Note the library and its version, the kernels it
chose, the CPU, its caches, and the frequency line. If it warns that the
governor is not `performance` or that turbo is on, say so with the
results: the numbers then move with the clock.

## 2. Pick the routine and its sizes

`list_routines` (or `man blas-microbenchmark`) says what each routine's
size options are. Choose sizes that cross the caches `--version` gave:

- Level 1 (vectors, `-v`): from inside L1 to well past the last-level
  cache, e.g. `-v 1024:16777216`. Expect GB/s to fall at each cache.
- Level 2 (`-m`, `-M`): from a few hundred to a few thousand, e.g.
  `-m 256:4096`.
- Level 3 (`-m`, and `-M`, `-K` for shapes): from a few hundred to a few
  thousand, e.g. `-m 256:2048`. Expect GFLOP/s to rise with the size.

Do not go past what the memory holds: `bmb_mcp` refuses such a run, and
a benchmark run by hand would just take the memory.

## 3. Threads

One thread first (`-t 1`), then the counts that matter, e.g. `-t 1:8`,
never more than the CPUs the process may use. Pin them for numbers that
repeat: `OMP_PROC_BIND=close OMP_PLACES=cores` for the OpenMP-threaded
libraries.

## 4. Run, and keep the result

```bash
bmb_dgemm -m 256:2048 -t 1:8 -o results/dgemm.json
```

or `run_benchmark` with `routine`, `dim1`, `threads`. Keep the checking
on: never pass `-C` (`verify: false`) unless the library is known right
and the user asked. Keep every result in a file (`-o`): the JSON records
how it was measured.

## 5. Report what was measured

- Give the rate the routine is judged by: GFLOP/s for level 3, GB/s
  (and GFLOP/s) for levels 1 and 2; copy, swap and `i?amax` have GB/s only.
- Give the IQR: above a few percent, something else was running. Repeat
  the run into another file before drawing conclusions.
- Repeat the warnings the benchmark gave, and the library and machine.
- For a page of charts, `bmb_report results > report.html` (`make_report`).
