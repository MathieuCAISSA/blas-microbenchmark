# ReFrame tests for blas-microbenchmark, one per routine: bmb_run on an
# installed copy, bmb_build_run on one built from a release tarball. How a
# site runs them, sets their sizes and their references:
# packaging/reframe/README.md.

import json
import os
import shlex

import reframe as rfm
import reframe.utility.sanity as sn
import reframe.utility.typecheck as typ
from reframe.core.builtins import (fixture, parameter, performance_function,
                                   run_after, run_before, sanity_function,
                                   variable)

# Every benchmark and its BLAS level, which picks its size option (-v or
# -m) and its directory under libexec. CI checks this against the
# benchmarks a build installs.
ROUTINES = {
    'dasum': 1, 'daxpy': 1, 'dcopy': 1, 'ddot': 1, 'dnrm2': 1, 'dscal': 1,
    'dswap': 1, 'saxpy': 1, 'sdot': 1, 'zdotc': 1,
    'dgemv': 2, 'dger': 2, 'dsymv': 2, 'dsyr': 2, 'dsyr2': 2, 'dtrmv': 2,
    'dtrsv': 2, 'sgemv': 2,
    'dgemm': 3, 'dsymm': 3, 'dsyrk': 3, 'dsyr2k': 3, 'dtrmm': 3, 'dtrsm': 3,
    'sgemm': 3, 'cgemm': 3, 'zgemm': 3, 'strsm': 3, 'ztrsm': 3, 'ssyrk': 3,
    'zherk': 3, 'zhemm': 3,
}

# The result file's rate fields, as ReFrame performance variables. A
# benchmark writes only those that apply: no GFLOP/s for dcopy and dswap,
# no GB/s at level 3.
RATES = {'gflops': 'GFLOP/s', 'gbytes_per_s': 'GB/s'}


class bmb_base(rfm.RunOnlyRegressionTest):
    '''One routine, at one size, on every core of a node: verified
    (--verify, the default), then timed. Subclasses say where the
    benchmark is.'''

    routine = parameter(sorted(ROUTINES))

    #: The size each level runs at, large enough that the operands leave
    #: the caches of a current server: the vector length at level 1 (two
    #: vectors of 32 MB), M = N = K at levels 2 and 3 (128 MB and 32 MB
    #: per matrix).
    level1_size = variable(int, value=4000000)
    level2_size = variable(int, value=4096)
    level3_size = variable(int, value=2048)

    #: Threads, through -t. 0: the partition's cores, as ReFrame knows
    #: them (its CPUs when it knows no cores, 1 when it knows neither).
    num_threads = variable(int, value=0)

    #: More options for the benchmark, such as ['-i', '50'].
    bmb_options = variable(typ.List[str], value=[])

    #: A JSON file of references, by system and routine (README.md); a
    #: relative path is from where reframe runs.
    reference_file = variable(str, value='')

    valid_systems = ['*']
    valid_prog_environs = ['*']
    num_tasks = 1
    keep_files = ['result.json']

    def bmb_executable(self, level):
        raise NotImplementedError

    @run_after('init')
    def describe(self):
        level = ROUTINES[self.routine]
        self.descr = f'blas-microbenchmark: {self.routine}'
        self.tags = {'blas', f'level{level}'}

    @run_before('run')
    def set_command(self):
        level = ROUTINES[self.routine]
        processor = self.current_partition.processor
        threads = (self.num_threads or processor.num_cores
                   or processor.num_cpus or 1)
        size = {1: self.level1_size, 2: self.level2_size,
                3: self.level3_size}[level]
        self.num_cpus_per_task = threads
        self.executable = self.bmb_executable(level)
        # A routine newer than the copy (strsm before 1.5.0) is skipped;
        # one looked up on PATH can only be found missing when it runs.
        self.skip_if(os.path.isabs(self.executable)
                     and not os.path.exists(self.executable),
                     f'no {self.routine} in this copy: {self.executable}')
        self.executable_opts = ['-v' if level == 1 else '-m', str(size),
                                '-t', str(threads), '-o', 'result.json',
                                *self.bmb_options]

    # Exit status 2 is a wrong result; the line says every point was
    # checked, which -C would turn off.
    @sanity_function
    def validate(self):
        return sn.all([
            sn.assert_eq(self.job.exitcode, 0),
            sn.assert_found(r'^# verified: every point', self.stdout),
            sn.assert_found(rf'"routine": "{self.routine}"', 'result.json'),
        ])

    @performance_function('GFLOP/s')
    def gflops(self):
        return self.rate('gflops')

    @performance_function('GB/s')
    def gbytes_per_s(self):
        return self.rate('gbytes_per_s')

    def rate(self, field):
        return sn.extractsingle(rf'^\s*"{field}": ([0-9.eE+-]+)',
                                os.path.join(self.stagedir, 'result.json'),
                                1, float)

    @run_before('performance')
    def set_perf_variables(self):
        with open(os.path.join(self.stagedir, 'result.json')) as f:
            point = json.load(f)['results'][0]

        self.perf_variables = {
            name: getattr(self, name)() for name in RATES if name in point
        }

    # {"system:partition": {"dgemm": {"gflops": [value, lower, upper]}}},
    # the thresholds as ReFrame takes them: fractions of the value, or
    # null for none. Read at init: ReFrame resolves the references for the
    # partition before the test runs.
    @run_after('init')
    def set_references(self):
        if not self.reference_file:
            return

        with open(os.path.abspath(self.reference_file)) as f:
            references = json.load(f)

        reference = {}
        for system, routines in references.items():
            rates = routines.get(self.routine, {})
            reference[system] = {
                name: (*rates[name], RATES[name]) for name in rates
            }

        # Assigned whole: ReFrame makes it a ScopedDict at once, where
        # self.reference[system] = ... would read system as a scope.
        self.reference = reference


@rfm.simple_test
class bmb_run(bmb_base):
    '''An installed copy, the first of: the one under install_prefix; the
    one this file was installed with (make install puts it in
    <prefix>/share/blas-microbenchmark/reframe, with the default
    --libexecdir); the benchmarks on PATH (spack load, or a module that
    puts them there).'''

    #: The --prefix it was installed with; '' is the install this file
    #: belongs to, if it was installed, and PATH otherwise.
    install_prefix = variable(str, value='')

    def installed_with(self):
        '''The prefix this file was installed under, or ''. ReFrame's
        prefix is the directory of the test file.'''
        top = os.path.normpath(os.path.join(self.prefix, '..', '..', '..'))
        if (os.path.basename(os.path.dirname(self.prefix)) == 'blas-microbenchmark'
                and os.path.isdir(os.path.join(top, 'libexec', 'blas-microbenchmark'))):
            return top

        return ''

    def bmb_executable(self, level):
        prefix = self.install_prefix or self.installed_with()
        if not prefix:
            return f'bmb_{self.routine}'

        return os.path.join(prefix, 'libexec', 'blas-microbenchmark',
                            f'level{level}', f'bmb_{self.routine}')


class bmb_tarball(rfm.CompileOnlyRegressionTest):
    '''A release tarball, built against the BLAS that blas_backend names,
    once per environment.'''

    #: The release to build.
    version = variable(str, value='1.6.0')

    #: A URL or a path to take the tarball from instead of the release;
    #: then it is not checked against the release's SHA256SUMS.
    tarball = variable(str, value='')

    #: --with-blas-backend.
    blas_backend = variable(str, value='auto')

    #: More configure options, such as ['--with-blas-libpath=DIR'].
    configure_options = variable(typ.List[str], value=[])

    valid_systems = ['*']
    valid_prog_environs = ['*']
    sourcesdir = None
    build_system = 'Autotools'

    @run_before('compile')
    def fetch(self):
        name = f'blas-microbenchmark-{self.version}.tar.gz'
        release = ('https://github.com/MathieuCAISSA/blas-microbenchmark/'
                   f'releases/download/v{self.version}')
        if not self.tarball:
            fetch = (f'curl -fsSLO {release}/{name} '
                     f'&& curl -fsSLO {release}/SHA256SUMS '
                     f'&& grep " {name}$" SHA256SUMS > {name}.sha256 '
                     f'&& sha256sum -c {name}.sha256')
        elif '://' in self.tarball:
            name = 'blas-microbenchmark.tar.gz'
            fetch = f'curl -fsSL -o {name} {shlex.quote(self.tarball)}'
        else:
            name = 'blas-microbenchmark.tar.gz'
            path = shlex.quote(os.path.abspath(self.tarball))
            fetch = f'cp {path} {name}'

        # Unpacked into src/ whatever the directory in the tarball is
        # called, and built in src/build. ReFrame sets srcdir itself, from
        # sourcepath, so the directories are given relative to the stage.
        self.prebuild_cmds = [
            f'{fetch} && mkdir src && tar xf {name} -C src --strip-components=1'
        ]
        self.build_system.configuredir = 'src'
        self.build_system.builddir = 'src/build'
        self.build_system.config_opts = [
            f'--with-blas-backend={self.blas_backend}',
            *self.configure_options
        ]
        self.build_system.max_concurrency = 8

    @sanity_function
    def validate(self):
        return sn.path_isfile(os.path.join(
            self.stagedir, 'src', 'build', 'src', 'c', 'level3', 'bmb_dgemm'))


@rfm.simple_test
class bmb_build_run(bmb_base):
    '''A copy built from a release tarball (bmb_tarball), in each
    environment.'''

    bmb_build = fixture(bmb_tarball, scope='environment')

    def bmb_executable(self, level):
        return os.path.join(self.bmb_build.stagedir, 'src', 'build', 'src',
                            'c', f'level{level}', f'bmb_{self.routine}')
