/* Checks the machine probe.
 *
 * Every field it fills is best effort by design: a container without sysfs
 * cache information, or an aarch64 kernel without a "model name" line, is a
 * legitimate "unknown", not a failure. So on the real machine this checks
 * only what any Linux box guarantees (uname, sysconf, the clock), and that
 * whatever *was* recorded is sane -- a cache of size 0 or level 9 would be
 * a parsing bug. Exact values are checked on fake /proc and /sys trees
 * (fixtures/machine/): the CPU string, NUMA nodes, caches, the frequency
 * governor and turbo, and the advice they get. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    fixture("does-not-exist", root, sizeof(root));
    bmb_machine_probe_at(root, &m);
    check_str(m.cpu, "", "nothing readable: no CPU name");
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

int main(void)
{
    const bmb_machine_t *m = bmb_machine();
    char buf[512];
    int i;

    test_fixtures();
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
