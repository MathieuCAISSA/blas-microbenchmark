---
name: blas-results
description: Read the results of blas-microbenchmark (its table, CSV or JSON, or bmb_report's page) and explain what they mean - time, median and IQR, GFLOP/s against GB/s, the peak and the roofline, and what makes two results comparable. Use when given such a result, or asked what a BLAS benchmark's numbers say.
---

# Read blas-microbenchmark results

Use `read_result` from `bmb_mcp` when you have it; otherwise read the JSON
(`-o result.json`) or the table directly.

## What a point is

- `time_s` is one call's time from the fastest sample; `median_s` and the
  quartiles (`q1_s`, `q3_s`) say how the samples spread. An IQR (q3 - q1,
  as a percentage of the median) above a few percent means something else
  ran during the measurement: repeat before concluding.
- `gflops` is the rate from the routine's operation count, `gbytes_per_s`
  from the bytes it must move. Level 3 is judged by GFLOP/s; levels 1 and
  2 are limited by memory, and their GB/s says more; copy, swap and
  `i?amax` have GB/s only.
- `verified: true` means each point was checked against a reference
  before it was timed. A result without it was run with `-C`: its
  numbers may be fast and wrong.

## What a result records

The library and its version (`backend`, `blas`), the kernels it chose,
the CPU, its caches and frequency settings, the date, and the run:
command, samples, threads, the CPUs it could use, the variables set, the
routine's arguments (`trans`, `uplo`, ...), layout, cache state and
padding; the integers the library takes (`blas_int_bits`, 64 in an ILP64
build); a GEMM's input and accumulation types where they are not its
output's (`types`: sbgemm's bf16 into fp32); and `run.star` for a run of
single-thread instances, one a core, whose rates are their sum. Use them
to say what was measured.

## Comparing two results

Two results are comparable when only what is being compared differs.
Not comparable without saying so:

- different machines, or the same machine under another frequency
  setting;
- another layout (`row`/`col`), cache state (`hot`/`cold`), padding,
  or other arguments (`trans NT` is another computation than `NN`);
- different sizes or thread counts;
- a `--star` run (independent instances, rates summed) against threads
  splitting one call, at the same count;
- sbgemm (bf16 inputs) against sgemm: other units, another accuracy.

A label (`-l`) names a condition the result does not record by itself.

## The peak and the roofline

Given the machine's peak (`--peak-gflops`, `--peak-gbs`, as the user
knows it, never guessed), the report draws a line at the peak on level
3's charts, with each point's percentage of it, and a roofline for levels
1 and 2: each point under the memory roof (bandwidth times FLOPs a byte)
and the compute roof. A level 1 or 2 point near the memory roof is as
good as the memory allows; level 3 near the peak is as good as the cores.
sbgemm is judged only by a peak its results were given, in bf16: the
report's own is double precision's, which says nothing of it.

## Reporting

Say the rate with its unit, the size and threads, the library and
version, the IQR, and whether it was verified. Never present a single
unrepeated run's small difference as a finding.
