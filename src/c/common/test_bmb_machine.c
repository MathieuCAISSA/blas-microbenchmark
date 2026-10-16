/* Checks the machine probe.
 *
 * Every field it fills is best effort by design: a container without sysfs
 * cache information, or an aarch64 kernel without a "model name" line, is a
 * legitimate "unknown", not a failure. So on the real machine this checks
 * only what any Linux box guarantees (uname, sysconf, the clock), and that
 * whatever *was* recorded is sane -- a cache of size 0 or level 9 would be
 * a parsing bug. Exact values are checked on fake /proc and /sys trees
 * (fixtures/machine/): the CPU string, NUMA nodes, caches, the frequency
 * governor and turbo, and the advice they get. Trees the kernel would
 * rarely or never write -- cache sizes without K, a missing file, a
 * malformed CPU list or quota -- are generated here, in a temporary
 * directory, each read as unknown rather than guessed at. */

#define _XOPEN_SOURCE 700

#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bmb_machine.h"

static int failures;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

/* "2026-10-02T07:21:37Z": exactly the ISO 8601 shape the JSON promises. */
static int is_iso_date(const char *s)
{
    static const char shape[] = "dddd-dd-ddTdd:dd:ddZ";
    size_t i;

    if (strlen(s) != sizeof(shape) - 1) {
        return 0;
    }
    for (i = 0; shape[i] != '\0'; i++) {
        if (shape[i] == 'd' ? (s[i] < '0' || s[i] > '9') : (s[i] != shape[i])) {
            return 0;
        }
    }
    return 1;
}

static void check_str(const char *got, const char *want, const char *what)
{
    char msg[512];

    if (strcmp(got, want) == 0) {
        check(1, what);
    } else {
        snprintf(msg, sizeof(msg), "%s: got \"%s\", want \"%s\"", what, got, want);
        check(0, msg);
    }
}

/* The fixtures live next to this file; Automake runs tests with srcdir in
 * the environment, so this works in a VPATH build too. */
static void fixture(const char *name, char *buf, size_t size)
{
    const char *srcdir = getenv("srcdir");

    snprintf(buf, size, "%s/fixtures/machine/%s", srcdir ? srcdir : ".", name);
}

/* The same probe, run over fake procfs/sysfs trees, so that the paths which
 * only exist on other machines are checked everywhere. */
static void test_fixtures(void)
{
    bmb_machine_t m;
    char root[512];
    char buf[512];

    fixture("x86", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check_str(m.cpu, "AMD EPYC 7763 64-Core Processor", "x86: model name, from the first processor");
    check(m.numa_nodes == 2, "x86: two NUMA nodes, and the 'online' file beside them not counted");
    check(m.cache_count == 4, "x86: four caches on cpu0");
    bmb_machine_describe_caches(&m, buf, sizeof(buf));
    check_str(buf, "L1d 32K, L1i 32K, L2 512K, L3 32M", "x86: cache sizes parsed, 32768K read as 32M");
    check(m.cache_count == 4 && m.caches[3].size_bytes == 33554432ULL,
          "x86: L3 size in bytes");
    check_str(m.governor, "performance", "x86: the governor, the same on both CPUs, said once");
    check(m.turbo == 0 && m.turbo_control == BMB_TURBO_INTEL_PSTATE, "x86: turbo off, through intel_pstate");
    bmb_machine_describe_frequency(&m, buf, sizeof(buf));
    check_str(buf, "governor performance, turbo off", "x86: frequency described");
    bmb_machine_frequency_advice(&m, buf, sizeof(buf));
    check_str(buf, "", "x86: a steady frequency, no advice");

    fixture("laptop", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check(m.turbo == 1 && m.turbo_control == BMB_TURBO_INTEL_PSTATE, "laptop: no_turbo 0 is turbo on");
    bmb_machine_describe_frequency(&m, buf, sizeof(buf));
    check_str(buf, "governor powersave, turbo on", "laptop: frequency described");
    bmb_machine_frequency_advice(&m, buf, sizeof(buf));
    check_str(buf, "The CPU frequency can change during the run (governor powersave, turbo on), "
              "so timings may vary from run to run. For stable numbers: sudo cpupower "
              "frequency-set -g performance, and echo 1 | sudo tee "
              "/sys/devices/system/cpu/intel_pstate/no_turbo.",
              "laptop: advice for both, turbo through intel_pstate");

    fixture("amd", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check_str(m.governor, "performance/schedutil",
              "amd: governors that differ, each once, sorted; the offline CPU skipped");
    check(m.turbo == 1 && m.turbo_control == BMB_TURBO_CPUFREQ_BOOST, "amd: boost 1 is turbo on");
    bmb_machine_frequency_advice(&m, buf, sizeof(buf));
    check_str(buf, "The CPU frequency can change during the run (governor performance/schedutil, "
              "turbo on), so timings may vary from run to run. For stable numbers: sudo cpupower "
              "frequency-set -g performance, and echo 0 | sudo tee "
              "/sys/devices/system/cpu/cpufreq/boost.",
              "amd: one CPU not on performance is enough; turbo through cpufreq/boost");

    /* The form that must never change: this string is part of the report's
     * series key, so a "nicer" rewrite would stop files from one machine
     * merging across versions (#1, decision 4). */
    fixture("aarch64", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check_str(m.cpu, "implementer 0x41, part 0xd49", "aarch64: raw implementer and part codes");
    check(m.numa_nodes == 0, "aarch64 fixture without sysfs: NUMA left unknown, not guessed as 1");
    check(m.cache_count == 0, "aarch64 fixture without sysfs: no caches invented");
    bmb_machine_describe_caches(&m, buf, sizeof(buf));
    check_str(buf, "", "no caches, empty description");

    bmb_machine_describe_frequency(&m, buf, sizeof(buf));
    check_str(buf, "", "aarch64 fixture: no frequency information, none described");
    bmb_machine_frequency_advice(&m, buf, sizeof(buf));
    check_str(buf, "", "aarch64 fixture: nothing known, no advice");

    check(m.usable_cpus == 0, "aarch64 fixture: no affinity or quota, usable CPUs unknown");

    fixture("container", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check(m.usable_cpus == 3, "container: an affinity of 5 CPUs, a quota of 2.5 rounded up to 3");

    fixture("slurm", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check(m.usable_cpus == 2, "slurm: no quota of its own, its parent's 2 apply under an affinity of 8");

    fixture("pinned", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check(m.usable_cpus == 1, "pinned: an affinity of 1 under a quota of 4; the v1 lines skipped");

    fixture("bad-quota", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check(m.usable_cpus == 4, "bad-quota: a period of -100000 is not read; the affinity of 4 stands");

    fixture("does-not-exist", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check_str(m.cpu, "", "nothing readable: no CPU name");
    check(m.usable_cpus == 0, "nothing readable: usable CPUs unknown");
    check(m.os[0] != '\0' && m.date[0] != '\0', "nothing readable: os and date still from the real system");
}

static void test_describe(void)
{
    bmb_machine_t m;
    char buf[512];

    memset(&m, 0, sizeof(m));
    snprintf(m.cpu, sizeof(m.cpu), "X");
    m.logical_cpus = 1;
    m.numa_nodes = 1;
    bmb_machine_describe_cpu(&m, buf, sizeof(buf));
    check_str(buf, "X (1 logical CPU, 1 NUMA node)", "describe: singular counts");

    m.logical_cpus = 64;
    m.numa_nodes = 2;
    bmb_machine_describe_cpu(&m, buf, sizeof(buf));
    check_str(buf, "X (64 logical CPUs, 2 NUMA nodes)", "describe: plural counts");

    m.logical_cpus = 0;
    m.numa_nodes = 0;
    bmb_machine_describe_cpu(&m, buf, sizeof(buf));
    check_str(buf, "X", "describe: a model and nothing else");

    m.cpu[0] = '\0';
    m.logical_cpus = 4;
    bmb_machine_describe_cpu(&m, buf, sizeof(buf));
    check_str(buf, "4 logical CPUs", "describe: counts without a model");

    m.logical_cpus = 0;
    m.numa_nodes = 2;
    bmb_machine_describe_cpu(&m, buf, sizeof(buf));
    check_str(buf, "2 NUMA nodes", "describe: NUMA nodes without a CPU count");
    m.numa_nodes = 0;

    m.logical_cpus = 0;
    bmb_machine_describe_cpu(&m, buf, sizeof(buf));
    check_str(buf, "", "describe: nothing known, nothing said");

    m.turbo = -1;
    snprintf(m.governor, sizeof(m.governor), "powersave");
    bmb_machine_describe_frequency(&m, buf, sizeof(buf));
    check_str(buf, "governor powersave", "frequency: a governor, turbo unknown");
    bmb_machine_frequency_advice(&m, buf, sizeof(buf));
    check_str(buf, "The CPU frequency can change during the run (governor powersave), so timings "
              "may vary from run to run. For stable numbers: sudo cpupower frequency-set -g "
              "performance.", "advice: the governor alone");

    m.governor[0] = '\0';
    m.turbo = 1;
    m.turbo_control = BMB_TURBO_CPUFREQ_BOOST;
    bmb_machine_describe_frequency(&m, buf, sizeof(buf));
    check_str(buf, "turbo on", "frequency: turbo, governor unknown");
    bmb_machine_frequency_advice(&m, buf, sizeof(buf));
    check_str(buf, "The CPU frequency can change during the run (turbo on), so timings may vary "
              "from run to run. For stable numbers: echo 0 | sudo tee "
              "/sys/devices/system/cpu/cpufreq/boost.", "advice: turbo alone");

    m.turbo = 0;
    bmb_machine_frequency_advice(&m, buf, sizeof(buf));
    check_str(buf, "", "advice: turbo off and no governor known, none");
}

/* A tree of small files in a temporary directory of the build directory
 * (where make check runs), removed once probed. */
static char tree_root[64];

static void tree_file(const char *rel, const char *content)
{
    char path[512];
    char *p;
    FILE *f;
    int fd;

    snprintf(path, sizeof(path), "%s/%s", tree_root, rel);
    for (p = strchr(path + strlen(tree_root) + 1, '/'); p != NULL; p = strchr(p + 1, '/')) {
        *p = '\0';
        mkdir(path, 0755);
        *p = '/';
    }
    /* Only its owner writes it, whatever the umask. */
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    f = (fd >= 0) ? fdopen(fd, "w") : NULL;
    if (f == NULL) {
        check(0, path);
        if (fd >= 0) {
            close(fd);
        }
        return;
    }
    fputs(content, f);
    fclose(f);
}

static int tree_remove_one(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void) st;
    (void) flag;
    (void) ftw;
    return remove(path);
}

static void tree_begin(void)
{
    snprintf(tree_root, sizeof(tree_root), "bmb_machine_XXXXXX");
    if (mkdtemp(tree_root) == NULL) {
        check(0, "a temporary directory for the generated trees");
        exit(1);
    }
}

static void tree_end(void)
{
    nftw(tree_root, tree_remove_one, 16, FTW_DEPTH | FTW_PHYS);
}

#define CACHE "sys/devices/system/cpu/cpu0/cache/"

/* The affinity and quota a status line and a cpu.max give, with the
 * process in the root cgroup. */
static long usable_cpus(const char *allowed, const char *cpu_max)
{
    bmb_machine_t m;
    char line[128];

    tree_begin();
    snprintf(line, sizeof(line), "Name:\tbmb_dgemm\nCpus_allowed_list:\t%s\n", allowed);
    tree_file("proc/self/status", line);
    tree_file("proc/self/cgroup", "0::/\n");
    if (cpu_max != NULL) {
        tree_file("sys/fs/cgroup/cpu.max", cpu_max);
    }
    bmb_machine_probe_at(tree_root, &m);
    tree_end();
    return m.usable_cpus;
}

static void test_generated(void)
{
    bmb_machine_t m;
    char buf[512];
    char long_cgroup[400];

    /* Sizes in bytes, M and G; the fourth cache has no type, and stops
     * the list there. */
    tree_begin();
    tree_file(CACHE "index0/level", "1\n");
    tree_file(CACHE "index0/type", "Data\n");
    tree_file(CACHE "index0/size", "64\n");
    tree_file(CACHE "index1/level", "2\n");
    tree_file(CACHE "index1/type", "Unified\n");
    tree_file(CACHE "index1/size", "2M\n");
    tree_file(CACHE "index2/level", "3\n");
    tree_file(CACHE "index2/type", "Unified\n");
    tree_file(CACHE "index2/size", "1G\n");
    tree_file(CACHE "index3/level", "4\n");
    /* Leading and trailing blanks are not part of a governor; an empty
     * file is no governor at all. */
    tree_file("sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "  performance \n");
    tree_file("sys/devices/system/cpu/cpu1/cpufreq/scaling_governor", "");
    tree_file("sys/devices/system/cpu/cpu2/cpufreq/scaling_governor", "performance\n");
    bmb_machine_probe_at(tree_root, &m);
    tree_end();
    check(m.cache_count == 3, "generated: three caches, the one without a type not read");
    bmb_machine_describe_caches(&m, buf, sizeof(buf));
    check_str(buf, "L1d 64B, L2 2M, L3 1G", "generated: sizes in bytes, M and G");
    bmb_machine_describe_caches(&m, buf, 13);
    check_str(buf, "L1d 64B", "generated: a cache that does not fit is left out whole");
    check_str(m.governor, "performance", "generated: a governor trimmed, an empty one skipped");

    tree_begin();
    tree_file(CACHE "index0/level", "1\n");
    tree_file(CACHE "index0/type", "InstructionAndDataAndMore\n");
    tree_file(CACHE "index0/size", "32K\n");
    tree_file(CACHE "index1/level", "2\n");
    tree_file(CACHE "index1/type", "Unified\n");
    bmb_machine_probe_at(tree_root, &m);
    tree_end();
    check(m.cache_count == 1 && strlen(m.caches[0].type) == sizeof(m.caches[0].type) - 1,
          "generated: a type too long for its field cut, still terminated; a cache without a size not read");

    /* What the affinity and the quota say when they do not parse: unknown,
     * never a guess. */
    check(usable_cpus("0-3", NULL) == 4, "generated: an affinity of 0-3 is 4 CPUs");
    check(usable_cpus("x", NULL) == 0, "generated: an affinity that is not a list, unknown");
    check(usable_cpus("3-1", NULL) == 0, "generated: a range that runs backwards, unknown");
    check(usable_cpus("0-", NULL) == 0, "generated: a range with no end, unknown");
    check(usable_cpus("0;1", NULL) == 0, "generated: a list with another separator, unknown");
    check(usable_cpus("0-3", "max 100000\n") == 4, "generated: cpu.max without a quota, the affinity stands");
    check(usable_cpus("0-3", "100000\n") == 4, "generated: cpu.max without a period, ignored");
    check(usable_cpus("0-3", "0 100000\n") == 4, "generated: a quota of 0, ignored");
    check(usable_cpus("0-3", "100000 0\n") == 4, "generated: a period of 0, ignored");
    check(usable_cpus("0-3", "100000 100000x\n") == 4, "generated: a period followed by junk, ignored");
    check(usable_cpus("0-3", "18446744073709551615 1\n") == 4,
          "generated: a quota too large for a long, ignored");
    check(usable_cpus("0-3", "100000 100000\n") == 1, "generated: a quota of one period is 1 CPU");

    /* A cgroup path too long to hold is not read at all: cut short, it
     * would lead to another cgroup's quota. */
    tree_begin();
    memset(long_cgroup, 'a', sizeof(long_cgroup));
    memcpy(long_cgroup, "0::/", 4);
    long_cgroup[sizeof(long_cgroup) - 2] = '\n';
    long_cgroup[sizeof(long_cgroup) - 1] = '\0';
    tree_file("proc/self/status", "Cpus_allowed_list:\t0-7\n");
    tree_file("proc/self/cgroup", long_cgroup);
    tree_file("sys/fs/cgroup/cpu.max", "100000 100000\n");
    bmb_machine_probe_at(tree_root, &m);
    tree_end();
    check(m.usable_cpus == 8, "generated: a cgroup path too long to hold, no quota read from a guess");
}

int main(void)
{
    const bmb_machine_t *m = bmb_machine();
    char buf[512];
    int i;

    test_fixtures();
    test_generated();
    test_describe();

    printf("     cpu=\"%s\" logical_cpus=%ld numa_nodes=%d caches=%d\n",
           m->cpu, m->logical_cpus, m->numa_nodes, m->cache_count);
    printf("     os=\"%s\" date=\"%s\" governor=\"%s\" turbo=%d\n", m->os, m->date, m->governor, m->turbo);

    check(bmb_machine() == m, "probed once, the same machine on every call");
    check(m->os[0] != '\0', "os from uname");
    check(m->logical_cpus > 0, "logical CPU count from sysconf");
    check(is_iso_date(m->date), "date in ISO 8601, UTC");

    for (i = 0; i < m->cache_count; i++) {
        const bmb_cache_t *c = &m->caches[i];
        char what[96];

        snprintf(what, sizeof(what), "cache %d is sane (level %d, %s, %llu bytes)",
                 i, c->level, c->type, c->size_bytes);
        check(c->level >= 1 && c->level <= 4 && c->size_bytes > 0
              && (strcmp(c->type, "data") == 0 || strcmp(c->type, "instruction") == 0
                  || strcmp(c->type, "unified") == 0),
              what);
    }

    bmb_machine_describe_cpu(m, buf, sizeof(buf));
    check(m->cpu[0] == '\0' || strstr(buf, m->cpu) == buf,
          "cpu description starts with the model");
    bmb_machine_describe_caches(m, buf, sizeof(buf));
    check((m->cache_count == 0) == (buf[0] == '\0'),
          "cache description is empty exactly when no cache was read");

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
