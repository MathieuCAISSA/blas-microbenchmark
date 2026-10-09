# Copyright Spack Project Developers. See COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

import os
import re
from glob import glob

from spack_repo.builtin.build_systems.autotools import AutotoolsPackage

from spack.package import *

# The BLAS providers the benchmarks can be built against, and the
# --with-blas-backend each one takes.
BACKENDS = {
    "openblas": "openblas",
    "blis": "blis",
    "netlib-lapack": "netlib",
    "nvpl-blas": "nvpl",
    "armpl-gcc": "armpl",
    "flexiblas": "flexiblas",
}


class BlasMicrobenchmark(AutotoolsPackage):
    """Command-line microbenchmarks for BLAS routines, in the manner of the
    OSU micro-benchmarks: one executable per routine, levels 1 to 3, with
    the same options for all of them, each result checked against a
    reference before it is timed, and a self-contained HTML report to
    compare libraries."""

    homepage = "https://mathieucaissa.github.io/blas-microbenchmark/"
    url = "https://github.com/MathieuCAISSA/blas-microbenchmark/releases/download/v1.4.0/blas-microbenchmark-1.4.0.tar.gz"
    git = "https://github.com/MathieuCAISSA/blas-microbenchmark.git"

    maintainers("MathieuCAISSA")

    license("Apache-2.0")

    version("main", branch="main")
    version("1.4.0", sha256="4731a242981cfbdb9f0e34786248ae14ca4528cf0f6410708cec980533d6da76")

    depends_on("c", type="build")
    depends_on("blas")

    # A git checkout has no configure: autoreconf makes it.
    depends_on("autoconf", type="build", when="@main")
    depends_on("automake", type="build", when="@main")

    requires(
        *(f"^[virtuals=blas] {provider}" for provider in BACKENDS),
        policy="one_of",
        msg="blas-microbenchmark is built against one of: " + ", ".join(BACKENDS),
    )
    # The benchmarks call BLIS through its CBLAS layer.
    requires("^blis +cblas", when="^[virtuals=blas] blis")

    def configure_args(self):
        blas = self.spec["blas"]
        args = [
            f"--with-blas-backend={BACKENDS[blas.name]}",
            f"--with-blas-libpath={blas.libs.directories[0]}",
        ]
        # Netlib's BLAS has no C interface: the package brings its own,
        # over the Fortran routines, and needs no cblas.h.
        if blas.name != "netlib-lapack":
            args.append(f"--with-blas-incpath={self._cblas_dir(blas)}")
        return args

    def _cblas_dir(self, blas):
        """The directory holding the provider's cblas.h, which several of
        them install in a subdirectory (include/blis, include/flexiblas)."""
        found = find(blas.prefix.include, "cblas.h", recursive=True)
        if not found:
            raise InstallError(f"no cblas.h under {blas.prefix.include}")
        return os.path.dirname(min(found, key=len))

    def setup_run_environment(self, env: EnvironmentModifications) -> None:
        # The benchmarks install by BLAS level under libexec, not in bin.
        for level in ("level1", "level2", "level3"):
            env.prepend_path(
                "PATH", join_path(self.prefix.libexec, "blas-microbenchmark", level)
            )

    def _benchmarks(self):
        return sorted(
            glob(join_path(self.prefix.libexec, "blas-microbenchmark", "level*", "bmb_*"))
        )

    def test_version(self):
        """check the version and the BLAS backend the benchmarks report"""
        dgemm = which(
            join_path(self.prefix.libexec, "blas-microbenchmark", "level3", "bmb_dgemm"),
            required=True,
        )
        out = dgemm("--version", output=str.split, error=str.split)
        if not self.spec.satisfies("@main"):
            expected = f"blas-microbenchmark {self.spec.version}"
            assert expected in out, f"no '{expected}' in {out}"
        backend = BACKENDS[self.spec["blas"].name]
        if backend == "flexiblas":
            backend = "flexiblas/"
        assert f"BLAS backend: {backend}" in out, f"no 'BLAS backend: {backend}' in {out}"

    def test_linked_library(self):
        """check the benchmarks run the BLAS of the spec, not another one installed"""
        blas = self.spec["blas"]
        dgemm = join_path(self.prefix.libexec, "blas-microbenchmark", "level3", "bmb_dgemm")
        out = which("ldd", required=True)(dgemm, output=str, error=str)
        prefix = os.path.realpath(blas.prefix)
        for name in blas.libs.names:
            lines = [l for l in out.splitlines() if re.search(rf"\blib{re.escape(name)}\.so", l)]
            assert lines, f"bmb_dgemm does not load lib{name}: {out}"
            for line in lines:
                path = line.split("=>")[-1].split("(")[0].strip()
                assert os.path.realpath(path).startswith(prefix), (
                    f"bmb_dgemm loads {path}, not the lib{name} of {blas.prefix}"
                )

    def test_benchmarks(self):
        """run every benchmark at a tiny size, its result checked against a reference"""
        benchmarks = self._benchmarks()
        assert len(benchmarks) >= 26, f"only {len(benchmarks)} benchmarks installed"
        for path in benchmarks:
            name = os.path.basename(path)
            level = os.path.basename(os.path.dirname(path))
            size = ["-v", "64"] if level == "level1" else ["-m", "16"]
            with test_part(self, f"test_benchmarks_{name}", purpose=f"{name} is verified"):
                bench = which(path, required=True)
                out = bench("-x", "0", "-i", "1", *size, output=str.split, error=str.split)
                assert "# verified: " in out, f"{name} did not check its result: {out}"

    def test_report(self):
        """turn a result file into a report"""
        ddot = which(
            join_path(self.prefix.libexec, "blas-microbenchmark", "level1", "bmb_ddot"),
            required=True,
        )
        ddot("-x", "0", "-i", "1", "-v", "8:32", "-o", "ddot.json", output=str.split)
        report = which(self.prefix.bin.bmb_report, required=True)
        out = report("ddot.json", output=str, error=str.split)
        assert out.startswith("<!doctype html>"), "bmb_report did not write an HTML page"
        assert '"routine": "ddot"' in out, "the page does not carry the result"

    def test_man_pages(self):
        """check the manual pages, an alias per benchmark included"""
        man1 = self.prefix.share.man.man1
        for page in ["blas-microbenchmark.1", "bmb_report.1"] + [
            os.path.basename(b) + ".1" for b in self._benchmarks()
        ]:
            assert os.path.isfile(join_path(man1, page)), f"no {page} in {man1}"
