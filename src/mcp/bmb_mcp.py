#!/usr/bin/env python3
"""bmb_mcp -- an MCP server over blas-microbenchmark, for agents.

    bmb_mcp [--results-dir DIR] [--time-cap SECONDS] [--memory-limit BYTES]

Speaks the Model Context Protocol over stdio (JSON-RPC 2.0, a message a
line): an agent lists the routines, describes the machine, runs a
benchmark or the suite, reads a result and makes the report, through
tools whose arguments are fields, never a command line (#262). Every run
is checked before it starts: its sizes against the memory, its threads
against the CPUs, every path inside the results directory; and it is
stopped at the time cap. Standard library only, so that it runs wherever
Python 3 does. This file is the source; make substitutes the version
and where the benchmarks are installed (@...@ below).
"""

import json
import os
import re
import subprocess
import sys

VERSION = '@PACKAGE_VERSION@'
LIBEXEC = '@pkglibexecdir@'

# The protocol revisions this server speaks, newest first: a client
# asking for one of them gets it, any other the newest.
PROTOCOLS = ('2025-06-18', '2025-03-26', '2024-11-05')

# BEGIN routines (mcp): written by src/c/routines.sh
ROUTINES = """
    sasum:1:s:v:asum dasum:1:d:v:asum scasum:1:c:v:asum dzasum:1:z:v:asum
    saxpy:1:s:v:axpy daxpy:1:d:v:axpy caxpy:1:c:v:axpy zaxpy:1:z:v:axpy
    saxpby:1:s:v:axpby daxpby:1:d:v:axpby caxpby:1:c:v:axpby zaxpby:1:z:v:axpby
    scopy:1:s:v:copy dcopy:1:d:v:copy ccopy:1:c:v:copy zcopy:1:z:v:copy
    sdot:1:s:v:dot ddot:1:d:v:dot cdotc:1:c:v:dotc zdotc:1:z:v:dotc
    cdotu:1:c:v:dotu zdotu:1:z:v:dotu snrm2:1:s:v:nrm2 dnrm2:1:d:v:nrm2
    scnrm2:1:c:v:nrm2 dznrm2:1:z:v:nrm2 sscal:1:s:v:scal dscal:1:d:v:scal
    cscal:1:c:v:scal zscal:1:z:v:scal sswap:1:s:v:swap dswap:1:d:v:swap
    cswap:1:c:v:swap zswap:1:z:v:swap isamax:1:s:v:iamax idamax:1:d:v:iamax
    icamax:1:c:v:iamax izamax:1:z:v:iamax srot:1:s:v:rot drot:1:d:v:rot
    sgemv:2:s:mn:gemv dgemv:2:d:mn:gemv cgemv:2:c:mn:gemv zgemv:2:z:mn:gemv
    sgbmv:2:s:nk:gbmv dgbmv:2:d:nk:gbmv sger:2:s:mn:ger dger:2:d:mn:ger
    cgeru:2:c:mn:geru zgeru:2:z:mn:geru cgerc:2:c:mn:gerc zgerc:2:z:mn:gerc
    ssymv:2:s:n:symv dsymv:2:d:n:symv chemv:2:c:n:hemv zhemv:2:z:n:hemv
    sspmv:2:s:n:spmv dspmv:2:d:n:spmv ssyr:2:s:n:syr dsyr:2:d:n:syr
    cher:2:c:n:her zher:2:z:n:her ssyr2:2:s:n:syr2 dsyr2:2:d:n:syr2
    cher2:2:c:n:her2 zher2:2:z:n:her2 strmv:2:s:n:trmv dtrmv:2:d:n:trmv
    ctrmv:2:c:n:trmv ztrmv:2:z:n:trmv strsv:2:s:n:trsv dtrsv:2:d:n:trsv
    ctrsv:2:c:n:trsv ztrsv:2:z:n:trsv stpsv:2:s:n:tpsv dtpsv:2:d:n:tpsv
    stbsv:2:s:nk:tbsv dtbsv:2:d:nk:tbsv sgemm:3:s:mkn:gemm dgemm:3:d:mkn:gemm
    cgemm:3:c:mkn:gemm zgemm:3:z:mkn:gemm sgemmt:3:s:nk:gemmt
    dgemmt:3:d:nk:gemmt cgemmt:3:c:nk:gemmt zgemmt:3:z:nk:gemmt
    ssymm:3:s:mn:symm dsymm:3:d:mn:symm csymm:3:c:mn:symm zsymm:3:z:mn:symm
    chemm:3:c:mn:hemm zhemm:3:z:mn:hemm ssyrk:3:s:nk:syrk dsyrk:3:d:nk:syrk
    csyrk:3:c:nk:syrk zsyrk:3:z:nk:syrk cherk:3:c:nk:herk zherk:3:z:nk:herk
    ssyr2k:3:s:nk:syr2k dsyr2k:3:d:nk:syr2k csyr2k:3:c:nk:syr2k
    zsyr2k:3:z:nk:syr2k cher2k:3:c:nk:her2k zher2k:3:z:nk:her2k
    strmm:3:s:mn:trmm dtrmm:3:d:mn:trmm ctrmm:3:c:mn:trmm ztrmm:3:z:mn:trmm
    strsm:3:s:mn:trsm dtrsm:3:d:mn:trsm ctrsm:3:c:mn:trsm ztrsm:3:z:mn:trsm
"""
# END routines (mcp)

SIZES = {
    'v': '-v (vector_size) is the vector size',
    'mn': '-m (dim1) is M, -M (dim2) is N',
    'mkn': '-m (dim1) is M and K, -M (dim2) is N; -K (dim3) gives K apart',
    'nk': '-m (dim1) is N, -M (dim2) is K',
    'n': '-m (dim1) is N (square; no -M)',
}
PRECISIONS = {'s': 'single', 'd': 'double', 'c': 'single complex',
              'z': 'double complex'}
DIRS = {'s': 'single', 'd': 'double', 'c': 'complex', 'z': 'complex16'}

SWEEP = r'^[0-9]+(:[0-9]+(:[0-9]+)?|(,[0-9]+)*)$'
NAME = r'^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$'

USAGE = """Usage: bmb_mcp [OPTION]...

An MCP server (stdio) over blas-microbenchmark, for AI agents: it lists the
routines, describes the machine, runs a benchmark or the suite, reads a
result and makes the report. A client starts it; it is not run by hand.

      --results-dir <dir>   where results and reports go, and the only
                            place a tool reads from (default: ./bmb-results)
      --time-cap <s>        the longest a run may take, in seconds
                            (default: 900)
      --memory-limit <b>    the most memory a run's operands may take, in
                            bytes (default: a quarter of the memory available)
  -h, --help                show this help
  -V, --version             show the version

Full documentation: man bmb_mcp
"""


def table():
    """The routines, by name: level, precision, sizes and family."""
    out = {}
    for word in ROUTINES.split():
        name, level, prec, sizes, family = word.split(':')
        out[name] = {'routine': name, 'level': int(level), 'precision': prec,
                     'sizes': sizes, 'family': family}
    return out


class Refused(Exception):
    """A request refused before anything ran, with what to do instead."""


class Server:
    def __init__(self, results_dir, time_cap, memory_limit):
        self.results = os.path.abspath(results_dir)
        self.time_cap = time_cap
        self.memory_limit = memory_limit
        self.routines = table()
        # Where the benchmarks, bmb_suite and bmb_report are: the build
        # tree this script is in, or the install it came with, as
        # bmb_suite finds its own.
        here = os.path.dirname(os.path.abspath(__file__))
        if (os.path.isfile(os.path.join(here, 'Makefile'))
                and os.path.isdir(os.path.join(here, '..', 'c', 'level1'))):
            self.suite = os.path.normpath(os.path.join(here, '..', 'c'))
            self.tree = True
            self.bmb_suite = os.path.join(here, '..', 'suite', 'bmb_suite')
            self.bmb_report = os.path.join(here, '..', 'report', 'bmb_report')
        else:
            self.suite = LIBEXEC
            self.tree = False
            self.bmb_suite = os.path.join(here, 'bmb_suite')
            self.bmb_report = os.path.join(here, 'bmb_report')

    # ---- what is on this machine ----

    def bench(self, routine):
        r = self.routines[routine]
        if self.tree:
            path = os.path.join(self.suite, 'level%d' % r['level'],
                                DIRS[r['precision']], 'bmb_' + routine)
        else:
            path = os.path.join(self.suite, 'level%d' % r['level'],
                                'bmb_' + routine)
        if not os.access(path, os.X_OK):
            raise Refused('bmb_%s is not in %s: install blas-microbenchmark '
                          '(https://github.com/MathieuCAISSA/blas-microbenchmark), '
                          'or start bmb_mcp from a build tree' % (routine, self.suite))
        return path

    @staticmethod
    def cpus():
        try:
            return len(os.sched_getaffinity(0))
        except (AttributeError, OSError):
            return os.cpu_count() or 1

    def memory(self):
        """The memory a run may take: the limit given, or a quarter of
        what /proc/meminfo says is available."""
        if self.memory_limit:
            return self.memory_limit
        try:
            with open('/proc/meminfo') as f:
                for line in f:
                    if line.startswith('MemAvailable:'):
                        return int(line.split()[1]) * 1024 // 4
        except OSError:
            pass
        return 1 << 30

    def inside(self, path):
        """path, relative to the results directory, if it is inside it."""
        full = os.path.realpath(os.path.join(self.results, path))
        root = os.path.realpath(self.results)
        if full != root and not full.startswith(root + os.sep):
            raise Refused('%s is outside the results directory, %s: give a '
                          'path inside it' % (path, self.results))
        return full

    @staticmethod
    def env():
        e = dict(os.environ)
        e['NO_COLOR'] = '1'
        return e

    def run(self, argv, env=None, out=None):
        """argv's exit status, standard output and standard error, stopped
        at the time cap."""
        try:
            p = subprocess.run(argv, stdin=subprocess.DEVNULL,
                               stdout=out if out else subprocess.PIPE,
                               stderr=subprocess.PIPE, env=env or self.env(),
                               timeout=self.time_cap, universal_newlines=True)
        except subprocess.TimeoutExpired:
            raise Refused('the run went past the time cap, %g s, and was '
                          'stopped: give fewer sizes, threads or samples, or '
                          'start bmb_mcp with a longer --time-cap' % self.time_cap)
        return p.returncode, (p.stdout or '') if not out else '', p.stderr

    # ---- the tools ----

    def list_routines(self, args):
        levels = args.get('level') or [1, 2, 3]
        precisions = args.get('precision') or list(PRECISIONS)
        family = args.get('family')
        rows = []
        for r in self.routines.values():
            if r['level'] in levels and r['precision'] in precisions \
                    and (not family or r['family'] == family):
                rows.append(dict(r, precision=PRECISIONS[r['precision']],
                                 sizes=SIZES[r['sizes']]))
        text = '\n'.join('%-7s level %d, %s, %s; %s' % (
            r['routine'], r['level'], r['precision'], r['family'], r['sizes'])
            for r in rows)
        return {'routines': rows}, text or 'no routine matches'

    def describe_machine(self, args):
        status, out, err = self.run([self.bench('ddot'), '--version'])
        if status != 0:
            raise Refused('bmb_ddot --version failed: %s' % err.strip())
        info = {}
        for line in out.splitlines():
            if ': ' in line:
                k, v = line.split(': ', 1)
                info[k.lower().replace(' ', '_')] = v
            elif line.startswith('blas-microbenchmark '):
                info['version'] = line.split(' ', 1)[1]
        info.update({'cpus_available': self.cpus(),
                     'memory_limit_bytes': self.memory(),
                     'results_dir': self.results, 'time_cap_s': self.time_cap})
        return info, out.strip() + '\nCPUs this process may use: %d' % self.cpus()

    FLAGS = [('vector_size', '-v'), ('dim1', '-m'), ('dim2', '-M'),
             ('dim3', '-K'), ('threads', '-t'), ('trans', '--trans'),
             ('uplo', '--uplo'), ('side', '--side'), ('diag', '--diag'),
             ('layout', '-L'), ('cache', '-k'), ('ld_pad', '-p'),
             ('align', '-a'), ('batch_count', '--batch-count'),
             ('batch_mode', '--batch-mode'), ('iterations', '-i'),
             ('warmup', '-x'), ('warmup_time', '-w'), ('min_time', '-T'),
             ('max_spread', '-e'), ('max_time', '-E'), ('label', '-l'),
             ('peak_gflops', '--peak-gflops'), ('peak_gbs', '--peak-gbs')]

    @staticmethod
    def sweep(text):
        """The values of a sweep, as the benchmarks expand it."""
        m = re.match(r'^(\d+):(\d+)(?::(\d+))?$', text)
        if m:
            lo, hi = int(m.group(1)), int(m.group(2))
            step = int(m.group(3)) if m.group(3) else 0
            out, v = [], lo
            while v < hi and v > 0 and len(out) < 1000:
                out.append(v)
                v = v + step if step else v * 2
            return out + [hi]
        return [int(v) for v in text.split(',')]

    def check_threads(self, threads):
        if threads is None:
            return
        most = max(self.sweep(threads))
        if most > self.cpus():
            raise Refused('%d threads asked for, but this process may use %d '
                          'CPUs: give at most %d' % (most, self.cpus(), self.cpus()))

    def run_benchmark(self, args):
        routine = args['routine']
        if routine not in self.routines:
            raise Refused('no routine %s: list_routines gives them' % routine)
        bench = self.bench(routine)
        self.check_threads(args.get('threads'))
        opts = []
        for field, flag in self.FLAGS:
            if field in args:
                opts += [flag, str(args[field])]
        if args.get('verify') is False:
            opts.append('-C')
        # Its counts first, which measure and allocate nothing: the
        # options are checked as the benchmark checks them, and each
        # point's operands sized against the memory.
        env = self.env()
        env['BMB_COUNTS'] = '1'
        status, _, err = self.run([bench] + opts, env=env)
        if status != 0:
            raise Refused('bmb_%s refused the arguments: %s' % (routine, err.strip()))
        need = 0
        for line in err.splitlines():
            m = re.search(r' bytes (\S+) footprint (\S+)$', line)
            if line.startswith('counts: ') and m:
                for v in m.groups():
                    if v != '-':
                        need = max(need, int(v))
        if need > self.memory():
            raise Refused('the largest point needs about %d MB of operands, '
                          'past the %d MB a run may take: give smaller sizes'
                          % (need >> 20, self.memory() >> 20))
        name = args.get('name')
        os.makedirs(self.results, exist_ok=True)
        if name:
            path = self.inside(name + '.json')
            if os.path.exists(path):
                raise Refused('%s.json is already in the results directory: '
                              'give another name' % name)
        else:
            n = 1
            while os.path.exists(os.path.join(self.results, '%s-%d.json' % (routine, n))):
                n += 1
            path = os.path.join(self.results, '%s-%d.json' % (routine, n))
        status, _, err = self.run([bench] + opts + ['-o', path])
        warnings = [l for l in err.splitlines() if l.strip()]
        if status == 77:
            raise Refused('bmb_%s skipped this run: %s' % (routine, ' '.join(warnings)))
        if status != 0:
            raise Refused('bmb_%s failed (exit %d): %s' % (routine, status, '\n'.join(warnings)))
        return self.summary(path, warnings)

    def summary(self, path, warnings=None):
        """A result file as a summary: the routine, the library, whether
        it was verified, and a row a point."""
        try:
            with open(path) as f:
                r = json.load(f)
        except (OSError, ValueError) as e:
            raise Refused('%s cannot be read as a result: %s' % (path, e))
        if not isinstance(r, dict) or 'routine' not in r or 'results' not in r:
            raise Refused('%s is not a blas-microbenchmark result' % path)
        rows = []
        for p in r['results']:
            row = {'threads': p.get('thread_count'), 'dim1': p.get('dim1')}
            for k in ('dim2', 'dim3'):
                if k in p:
                    row[k] = p[k]
            row['time_s'] = p.get('time_s')
            if p.get('median_s'):
                row['median_s'] = p['median_s']
                if 'q1_s' in p and 'q3_s' in p:
                    row['iqr_percent'] = round(100 * (p['q3_s'] - p['q1_s']) / p['median_s'], 1)
            for k in ('gflops', 'gbytes_per_s'):
                if k in p:
                    row[k] = p[k]
            rows.append(row)
        out = {'file': os.path.relpath(path, self.results), 'routine': r['routine'],
               'backend': r.get('backend'), 'blas': r.get('blas'),
               'label': r.get('label'), 'verified': r.get('verified') is True,
               'arguments': (r.get('run') or {}).get('arguments'),
               'rows': rows, 'warnings': warnings or []}
        lines = ['%s, %s%s, %s, in %s' % (
            r['routine'], r.get('backend'), ' (%s)' % r['label'] if r.get('label') else '',
            'verified' if out['verified'] else 'NOT verified', out['file'])]
        for row in rows:
            size = ' x '.join(str(row[k]) for k in ('dim1', 'dim2', 'dim3') if k in row)
            rate = ', '.join('%g %s' % (row[k], u) for k, u in
                             (('gflops', 'GFLOP/s'), ('gbytes_per_s', 'GB/s')) if k in row)
            lines.append('  %s threads, %s: %.3g s%s, %s' % (
                row['threads'], size, row['time_s'],
                ' (IQR %g%%)' % row['iqr_percent'] if 'iqr_percent' in row else '', rate))
        lines += ['  warning: ' + w for w in out['warnings']]
        return out, '\n'.join(lines)

    def read_result(self, args):
        return self.summary(self.inside(args['path']))

    SUITE_FLAGS = [('threads', '-t'), ('layout', '-L'), ('cache', '-k'),
                   ('iterations', '-i'), ('warmup_time', '-w'),
                   ('min_time', '-T'), ('max_time', '-E')]

    def run_suite(self, args):
        name = args['name']
        directory = self.inside(name)
        if os.path.exists(directory) and os.listdir(directory):
            raise Refused('%s already holds results: give another name' % name)
        self.check_threads(args.get('threads'))
        argv = [self.bmb_suite]
        for key, flag in (('level', '--level'), ('precision', '--precision'),
                          ('routine', '--routine')):
            if args.get(key):
                argv += [flag, ','.join(str(v) for v in args[key])]
        if args.get('profile'):
            argv += ['--profile', args['profile']]
        for b in args.get('blas') or []:
            argv += ['--blas', '%s=%s' % (b['name'], b['suite'])]
        if args.get('flexiblas'):
            argv += ['--blas', 'flexiblas:' + ','.join(args['flexiblas'])]
        argv += ['-o', directory]
        opts = []
        for field, flag in self.SUITE_FLAGS:
            if field in args:
                opts += [flag, str(args[field])]
        if opts:
            argv += ['--'] + opts
        os.makedirs(self.results, exist_ok=True)
        status, _, err = self.run(argv)
        said = [l for l in err.splitlines() if l.strip()]
        if status == 2:
            raise Refused('bmb_suite refused: %s' % '\n'.join(said))
        files = []
        for top, _, names in os.walk(directory):
            files += [os.path.join(top, f) for f in sorted(names) if f.endswith('.json')]
        results = [self.summary(f)[0] for f in sorted(files)]
        brief = []
        for r in results:
            best = max(([row.get('gflops') or 0, row.get('gbytes_per_s') or 0] for row in r['rows']),
                       default=[0, 0])
            brief.append({'file': r['file'], 'routine': r['routine'], 'backend': r['backend'],
                          'verified': r['verified'], 'points': len(r['rows']),
                          'best_gflops': best[0] or None, 'best_gbytes_per_s': best[1] or None})
        out = {'directory': os.path.relpath(directory, self.results), 'results': brief,
               'failed': status != 0, 'said': said}
        text = ['%d results in %s%s' % (len(brief), out['directory'],
                                       ', some benchmarks failed' if status else '')]
        text += ['  %s (%s): %d points, best %s' % (
            b['file'], b['backend'], b['points'],
            '%g GFLOP/s' % b['best_gflops'] if b['best_gflops'] else '%g GB/s' % (b['best_gbytes_per_s'] or 0))
            for b in brief]
        if status:
            text += ['  ' + l for l in said if 'failed' in l or 'skipped' in l]
        return out, '\n'.join(text)

    def make_report(self, args):
        source = self.inside(args.get('directory', '.'))
        page = self.inside((args.get('name') or 'report') + '.html')
        argv = [self.bmb_report]
        for field, flag in (('peak_gflops', '--peak-gflops'), ('peak_gbs', '--peak-gbs')):
            if field in args:
                argv += [flag, str(args[field])]
        argv.append(source)
        with open(page + '.tmp', 'w') as out:
            status, _, err = self.run(argv, out=out)
        if status != 0:
            os.unlink(page + '.tmp')
            raise Refused('bmb_report refused: %s' % err.strip())
        os.replace(page + '.tmp', page)
        return {'page': page}, 'the report is %s: open it in a browser' % page

    # ---- the protocol ----

    def tools(self):
        sweep = {'type': 'string', 'pattern': SWEEP,
                 'description': 'a size, a doubling min:max, a step min:max:step, or a list a,b,c'}
        letters = lambda d, e: {'type': 'string', 'enum': e, 'description': d}
        number = lambda d: {'type': 'number', 'exclusiveMinimum': 0, 'description': d}
        integer = lambda d: {'type': 'integer', 'minimum': 0, 'description': d}
        name = {'type': 'string', 'pattern': NAME}
        run = {
            'routine': {'type': 'string', 'description': 'a routine, e.g. dgemm (list_routines gives them)'},
            'vector_size': dict(sweep, description='-v: the vector size of a level 1 routine'),
            'dim1': dict(sweep, description='-m: the first matrix dimension (list_routines says which)'),
            'dim2': dict(sweep, description='-M: the second (default: dim1)'),
            'dim3': dict(sweep, description='-K: GEMM\'s K apart from M'),
            'threads': dict(sweep, description='-t: the thread counts (default: 1)'),
            'trans': {'type': 'string', 'pattern': '^[NTCntc]{1,2}$',
                      'description': '--trans: N, T or C; two letters for GEMM, A\'s then B\'s'},
            'uplo': letters('--uplo: the triangle read', ['U', 'L']),
            'side': letters('--side: which side the matrix multiplies', ['L', 'R']),
            'diag': letters('--diag: a unit diagonal, not read', ['N', 'U']),
            'layout': letters('-L: storage order', ['row', 'col']),
            'cache': letters('-k: hot, or cold, rotated over copies past the last-level cache', ['hot', 'cold']),
            'ld_pad': integer('-p: elements added to each leading dimension'),
            'align': integer('-a: operand alignment in bytes, a power of two'),
            'batch_count': integer('--batch-count: matrices per call (GEMM)'),
            'batch_mode': letters('--batch-mode', ['loop', 'group', 'strided']),
            'iterations': integer('-i: samples taken (default 10)'),
            'warmup': integer('-x: untimed calls first (default 1)'),
            'warmup_time': {'type': 'number', 'minimum': 0, 'description': '-w: seconds of warm-up (default 0.2)'},
            'min_time': {'type': 'number', 'minimum': 0, 'description': '-T: sample until this many seconds'},
            'max_spread': {'type': 'number', 'minimum': 0, 'description': '-e: sample until the IQR is this percent of the median'},
            'max_time': number('-E: stop sampling at this many seconds'),
            'label': {'type': 'string', 'description': '-l: tag the results, e.g. "turbo off"'},
            'peak_gflops': number('--peak-gflops: a core\'s peak in GFLOP/s, as known'),
            'peak_gbs': number('--peak-gbs: the memory bandwidth in GB/s, as known'),
            'verify': {'type': 'boolean', 'description': 'check each point against a reference first (default: true); false only to time a library known to be right'},
            'name': dict(name, description='the result file\'s name, without .json (default: <routine>-<n>)'),
        }
        obj = lambda props, req: {'type': 'object', 'properties': props,
                                  'required': req, 'additionalProperties': False}
        lists = {'type': 'array', 'items': {'type': 'string'}}
        return [
            {'name': 'list_routines',
             'description': 'The routines this suite measures: each one\'s level, precision, family, and what its size options are.',
             'inputSchema': obj({'level': {'type': 'array', 'items': {'type': 'integer', 'enum': [1, 2, 3]}},
                                 'precision': {'type': 'array', 'items': {'type': 'string', 'enum': list(PRECISIONS)}},
                                 'family': {'type': 'string', 'description': 'e.g. gemm'}}, [])},
            {'name': 'describe_machine',
             'description': 'The BLAS library and its version, the kernels it chose, the CPU, caches, frequency and OS, the CPUs and memory a run may use.',
             'inputSchema': obj({}, [])},
            {'name': 'run_benchmark',
             'description': 'Run one routine\'s benchmark, verified, into a JSON result in the results directory, and return a row a point: time, median, IQR, GFLOP/s, GB/s, and the warnings. Refused before it runs if it needs more memory, threads or time than allowed.',
             'inputSchema': obj(run, ['routine'])},
            {'name': 'run_suite',
             'description': 'Run bmb_suite: every benchmark, or some levels, precisions or routines, at a profile\'s sizes (quick: about a minute; full: several), for one library or several (blas, flexiblas), into a directory of the results directory.',
             'inputSchema': obj({
                 'name': dict(name, description='the directory, in the results directory'),
                 'level': {'type': 'array', 'items': {'type': 'integer', 'enum': [1, 2, 3]}},
                 'precision': {'type': 'array', 'items': {'type': 'string', 'enum': list(PRECISIONS)}},
                 'routine': lists,
                 'profile': {'type': 'string', 'enum': ['quick', 'full']},
                 'blas': {'type': 'array', 'items': obj({'name': name, 'suite': {'type': 'string', 'description': 'an install prefix or a build tree'}}, ['name', 'suite'])},
                 'flexiblas': lists,
                 'threads': run['threads'], 'layout': run['layout'], 'cache': run['cache'],
                 'iterations': run['iterations'], 'warmup_time': run['warmup_time'],
                 'min_time': run['min_time'], 'max_time': run['max_time']}, ['name'])},
            {'name': 'read_result',
             'description': 'Summarise a result file of the results directory, as run_benchmark does.',
             'inputSchema': obj({'path': {'type': 'string', 'description': 'relative to the results directory'}}, ['path'])},
            {'name': 'make_report',
             'description': 'Make bmb_report\'s page of a directory of results: charts against size, libraries side by side, thread scaling, the roofline given a peak. Returns the page\'s path.',
             'inputSchema': obj({'directory': {'type': 'string', 'description': 'relative to the results directory (default: all of it)'},
                                 'name': dict(name, description='the page\'s name, without .html (default: report)'),
                                 'peak_gflops': run['peak_gflops'], 'peak_gbs': run['peak_gbs']}, [])},
        ]

    def check(self, schema, args):
        """args against a tool's schema: the fields it has, of their types."""
        if not isinstance(args, dict):
            raise Refused('the arguments are an object')
        props = schema['properties']
        for k in schema['required']:
            if k not in args:
                raise Refused('%s is required' % k)
        for k, v in args.items():
            if k not in props:
                raise Refused('no argument %s; the tool takes %s' % (k, ', '.join(sorted(props)) or 'none'))
            p = props[k]
            t = p.get('type')
            ok = {'string': isinstance(v, str), 'boolean': isinstance(v, bool),
                  'integer': isinstance(v, int) and not isinstance(v, bool),
                  'number': isinstance(v, (int, float)) and not isinstance(v, bool),
                  'array': isinstance(v, list), 'object': isinstance(v, dict)}.get(t, True)
            if not ok:
                raise Refused('%s is a %s' % (k, t))
            if t == 'string' and 'pattern' in p and not re.match(p['pattern'], v):
                raise Refused('%s: %r is not what it takes (%s)' % (k, v, p.get('description', p['pattern'])))
            if t == 'string' and p.get('enum') and v not in p['enum']:
                raise Refused('%s takes one of %s' % (k, ', '.join(p['enum'])))
            if t == 'array':
                items = p.get('items', {})
                for x in v:
                    if items.get('type') == 'object':
                        self.check(items, x)
                    elif items.get('enum') and x not in items['enum']:
                        raise Refused('%s takes values among %s' % (k, items['enum']))
                    elif items.get('type') == 'string' and not isinstance(x, str):
                        raise Refused('%s is a list of strings' % k)

    def call(self, name, args):
        tools = {t['name']: t for t in self.tools()}
        if name not in tools:
            return None
        try:
            self.check(tools[name]['inputSchema'], args)
            data, text = getattr(self, name)(args)
        except Refused as e:
            return {'content': [{'type': 'text', 'text': str(e)}], 'isError': True}
        return {'content': [{'type': 'text', 'text': text}],
                'structuredContent': data, 'isError': False}

    def handle(self, msg):
        """The response to one message, or None for a notification."""
        if not isinstance(msg, dict) or msg.get('jsonrpc') != '2.0' or not isinstance(msg.get('method'), str):
            return error(msg.get('id') if isinstance(msg, dict) else None, -32600, 'not a JSON-RPC 2.0 request')
        method, mid = msg['method'], msg.get('id')
        if 'id' not in msg:
            return None
        params = msg.get('params') or {}
        if method == 'initialize':
            asked = params.get('protocolVersion')
            return reply(mid, {
                'protocolVersion': asked if asked in PROTOCOLS else PROTOCOLS[0],
                'capabilities': {'tools': {}},
                'serverInfo': {'name': 'bmb_mcp', 'version': VERSION},
                'instructions': 'BLAS benchmarks. Call describe_machine first, then '
                                'list_routines; run_benchmark checks every result unless '
                                'told not to. Results and reports go to %s.' % self.results})
        if method == 'ping':
            return reply(mid, {})
        if method == 'tools/list':
            return reply(mid, {'tools': self.tools()})
        if method == 'tools/call':
            result = self.call(params.get('name'), params.get('arguments') or {})
            if result is None:
                return error(mid, -32602, 'no tool %s: tools/list gives them' % params.get('name'))
            return reply(mid, result)
        return error(mid, -32601, 'no method %s' % method)


def reply(mid, result):
    return {'jsonrpc': '2.0', 'id': mid, 'result': result}


def error(mid, code, message):
    return {'jsonrpc': '2.0', 'id': mid, 'error': {'code': code, 'message': message}}


def option(argv, i, name):
    if i + 1 >= len(argv):
        sys.exit('bmb_mcp: %s needs a value' % name)
    return argv[i + 1]


def main(argv):
    results, cap, memory = 'bmb-results', 900.0, 0
    i = 1
    while i < len(argv):
        a = argv[i]
        if a in ('-h', '--help'):
            sys.stdout.write(USAGE)
            return 0
        if a in ('-V', '--version'):
            print('bmb_mcp %s' % VERSION)
            return 0
        key, _, value = a.partition('=')
        if key in ('--results-dir', '--time-cap', '--memory-limit'):
            if not value:
                value = option(argv, i, key)
                i += 1
            try:
                if key == '--results-dir':
                    results = value
                elif key == '--time-cap':
                    cap = float(value)
                    if not 0 < cap <= 86400:
                        raise ValueError
                else:
                    memory = int(value)
                    if memory <= 0:
                        raise ValueError
            except ValueError:
                sys.exit('bmb_mcp: %s takes a number above 0 (got %r)' % (key, value))
        else:
            sys.stderr.write('bmb_mcp: unknown option: %s\n%s' % (a, USAGE))
            return 2
        i += 1
    if sys.stdin.isatty():
        sys.stderr.write('bmb_mcp: an MCP client starts this server and talks to it '
                         'on its standard input; see man bmb_mcp\n')
        return 2
    server = Server(results, cap, memory)
    for line in sys.stdin:
        if not line.strip():
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            out = error(None, -32700, 'not JSON')
        else:
            if isinstance(msg, list):
                out = [r for r in (server.handle(m) for m in msg) if r is not None] or None
            else:
                out = server.handle(msg)
        if out is not None:
            sys.stdout.write(json.dumps(out) + '\n')
            sys.stdout.flush()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
