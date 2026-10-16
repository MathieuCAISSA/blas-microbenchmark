# ReFrame tests

[ReFrame](https://reframe-hpc.readthedocs.io/) tests for
blas-microbenchmark, for sites that check their systems with it, written
for ReFrame 4 and run in CI with 4.10. `make install` puts this file and
`blas_microbenchmark.py` in `<prefix>/share/blas-microbenchmark/reframe`,
beside the benchmarks; it is also in the release tarball and in this
repository. `blas_microbenchmark.py` holds one test per routine, in two
families:

- **`bmb_run`** runs an installed copy, the first of: the one under
  `install_prefix`; the one the file was installed with, when it is run
  from `<prefix>/share/blas-microbenchmark/reframe`; the benchmarks on
  `PATH` (after `spack load blas-microbenchmark`, or a module that puts
  them there).
- **`bmb_build_run`** builds a release tarball first (`bmb_tarball`, once
  per programming environment), against the BLAS that `configure` finds,
  then runs that.

Each test runs one routine at one size on one node, with `-t` set to the
partition's cores. **Sanity** is the benchmark's own check: it exits 0,
and its output says every point was verified against a reference
(`--verify`, the default). The **performance variables** are the rates
the benchmark measures: `gflops` (GFLOP/s), and `gbytes_per_s` (GB/s) at
levels 1 and 2. dcopy and dswap have only the second, level 3 only the
first. Each test is tagged `blas` and `level1`, `level2` or `level3`.

## Running them

```bash
R=/usr/local/share/blas-microbenchmark/reframe   # wherever it was installed
reframe -C site-config.py -c $R/blas_microbenchmark.py -n bmb_run -r
```

`-S` sets the tests' variables:

| Variable | Default | |
|---|---|---|
| `install_prefix` | this file's install, or `PATH` | where it was installed (`bmb_run`) |
| `level1_size` | 4000000 | the vector length at level 1 |
| `level2_size`, `level3_size` | 4096, 2048 | M = N = K at levels 2 and 3 |
| `num_threads` | the partition's cores | `-t` |
| `bmb_options` | none | more options, such as `-S bmb_options=-i,50` |
| `reference_file` | none | the references, below |
| `modules`, `prerun_cmds` | none | ReFrame's own: `-S modules=blas-microbenchmark`, or `-S "prerun_cmds=spack load blas-microbenchmark"` |

For `bmb_build_run`, the fixture's own variables, set as
`-S bmb_tarball.<variable>=<value>`:

| Variable | Default | |
|---|---|---|
| `version` | 1.6.0 | the release to download, checked against its `SHA256SUMS` |
| `tarball` | the release's | a URL or a path to take it from instead, unchecked |
| `blas_backend` | `auto` | `--with-blas-backend`; `aocl` and `mkl` need a `version` from 1.5.0 on, and the library found (`AOCL_ROOT`, `MKLROOT`, or `configure_options`) |
| `configure_options` | none | more `configure` options, comma-separated |

A routine the copy does not have (`strsm` before 1.5.0) is skipped when
its path is known: under `install_prefix`, or built. On `PATH`, it fails,
so leave it out with `-x`.

`-n 'bmb_run%routine=dgemm'` selects one routine; `-t level3` one level.

## References

Every site's numbers are its own, so none ship here. Give them in a JSON
file, by system (or `system:partition`, or `*`), routine and performance
variable, as ReFrame takes them: the value, then the lower and upper
thresholds as fractions of it, `null` for none:

```json
{
  "mycluster:cpu": {
    "dgemm": {"gflops": [2900, -0.05, null]},
    "daxpy": {"gflops": [10, -0.1, null], "gbytes_per_s": [120, -0.1, null]}
  }
}
```

```bash
reframe -c $R/blas_microbenchmark.py -n bmb_run -S reference_file=references.json -r
```

A test without a reference for a variable still reports it, and passes.

## The results, as a report

Each test keeps the benchmark's `result.json` in ReFrame's output
directory, and `bmb_report` takes any number of them:

```bash
bmb_report $(find output -name result.json) > report.html
```
