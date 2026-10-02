/* Checks the machine probe.
 *
 * Every field it fills is best effort by design: a container without sysfs
 * cache information, or an aarch64 kernel without a "model name" line, is a
 * legitimate "unknown", not a failure. So this checks only what any Linux
 * box guarantees (uname, sysconf, the clock), and that whatever *was*
 * recorded is sane -- a cache of size 0 or level 9 would be a parsing bug. */

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
    printf("     os=\"%s\" date=\"%s\"\n", m->os, m->date);

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
