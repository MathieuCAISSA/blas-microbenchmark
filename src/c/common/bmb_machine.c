#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "bmb_machine.h"

static void bmb_trim(char *s)
{
    size_t n = strlen(s);
    char *start = s;

    while (n > 0 && isspace((unsigned char) s[n - 1])) {
        s[--n] = '\0';
    }
    while (*start != '\0' && isspace((unsigned char) *start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

/* Reads the first line of a small sysfs/procfs file. Returns 0 on success. */
static int bmb_read_line(const char *path, char *buf, size_t size)
{
    FILE *f = fopen(path, "r");

    if (f == NULL) {
        return -1;
    }
    if (fgets(buf, (int) size, f) == NULL) {
        fclose(f);
        return -1;
    }
    fclose(f);
    bmb_trim(buf);
    return 0;
}

/* x86 kernels give a readable "model name". aarch64 ones usually do not:
 * they give the implementer and part codes that `lscpu` decodes through a
 * table of its own. Those codes are recorded raw rather than guessed at --
 * they still tell two different CPUs apart, which is what the series key
 * needs. */
static void bmb_probe_cpu(bmb_machine_t *m, const char *root)
{
    char path[512];
    FILE *f;
    char line[512];
    char implementer[32] = "";
    char part[32] = "";

    snprintf(path, sizeof(path), "%s/proc/cpuinfo", root);
    f = fopen(path, "r");
    if (f == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        char *colon = strchr(line, ':');
        char *value;

        if (colon == NULL) {
            continue;
        }
        *colon = '\0';
        value = colon + 1;
        bmb_trim(line);
        bmb_trim(value);

        if (strcmp(line, "model name") == 0 && m->cpu[0] == '\0') {
            snprintf(m->cpu, sizeof(m->cpu), "%s", value);
            break;
        }
        if (strcmp(line, "CPU implementer") == 0 && implementer[0] == '\0') {
            snprintf(implementer, sizeof(implementer), "%s", value);
        } else if (strcmp(line, "CPU part") == 0 && part[0] == '\0') {
            snprintf(part, sizeof(part), "%s", value);
        }
    }
    fclose(f);

    if (m->cpu[0] == '\0' && implementer[0] != '\0' && part[0] != '\0') {
        snprintf(m->cpu, sizeof(m->cpu), "implementer %s, part %s", implementer, part);
    }
}

static void bmb_probe_numa(bmb_machine_t *m, const char *root)
{
    char path[512];
    DIR *dir;
    const struct dirent *entry;
    int nodes = 0;

    snprintf(path, sizeof(path), "%s/sys/devices/system/node", root);
    dir = opendir(path);

    /* No such directory means the kernel was built without NUMA support.
     * That is not quite the same as "1 node", so it is left unknown. */
    if (dir == NULL) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "node", 4) == 0
            && isdigit((unsigned char) entry->d_name[4])) {
            nodes++;
        }
    }
    closedir(dir);
    m->numa_nodes = nodes;
}

static unsigned long long bmb_parse_size(const char *s)
{
    char *end;
    unsigned long long value = strtoull(s, &end, 10);

    switch (*end) {
    case 'K':
        return value << 10;
    case 'M':
        return value << 20;
    case 'G':
        return value << 30;
    default:
        return value;
    }
}

/* cpu0's caches. On a hybrid CPU (performance and efficiency cores) the
 * other cores may differ; cpu0 is normally a performance core, which is
 * where a single-threaded benchmark runs anyway. */
static void bmb_probe_caches(bmb_machine_t *m, const char *root)
{
    int i;

    for (i = 0; i < BMB_MACHINE_MAX_CACHES; i++) {
        char path[512];
        char level[16], type[32], size[32];
        bmb_cache_t *c = &m->caches[m->cache_count];
        size_t k;

        snprintf(path, sizeof(path), "%s/sys/devices/system/cpu/cpu0/cache/index%d/level", root, i);
        if (bmb_read_line(path, level, sizeof(level)) != 0) {
            break;
        }
        snprintf(path, sizeof(path), "%s/sys/devices/system/cpu/cpu0/cache/index%d/type", root, i);
        if (bmb_read_line(path, type, sizeof(type)) != 0) {
            break;
        }
        snprintf(path, sizeof(path), "%s/sys/devices/system/cpu/cpu0/cache/index%d/size", root, i);
        if (bmb_read_line(path, size, sizeof(size)) != 0) {
            break;
        }

        c->level = atoi(level);
        for (k = 0; type[k] != '\0' && k + 1 < sizeof(c->type); k++) {
            c->type[k] = (char) tolower((unsigned char) type[k]);
        }
        c->type[k] = '\0';
        c->size_bytes = bmb_parse_size(size);
        if (c->level > 0 && c->size_bytes > 0) {
            m->cache_count++;
        }
    }
}

static int bmb_compare_names(const void *a, const void *b)
{
    return strcmp((const char *) a, (const char *) b);
}

/* The governor of every CPU that has one (an offline CPU has none). They
 * are normally all the same; when they are not, the distinct ones are kept,
 * sorted, since readdir's order is no order at all. */
static void bmb_probe_governor(bmb_machine_t *m, const char *root)
{
    char path[512];
    char seen[16][32];
    int count = 0;
    DIR *dir;
    const struct dirent *entry;
    size_t used = 0;
    int i;

    snprintf(path, sizeof(path), "%s/sys/devices/system/cpu", root);
    dir = opendir(path);
    if (dir == NULL) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        char governor[32];
        int known = 0;

        if (strncmp(entry->d_name, "cpu", 3) != 0 || !isdigit((unsigned char) entry->d_name[3])) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/sys/devices/system/cpu/%s/cpufreq/scaling_governor",
                 root, entry->d_name);
        if (bmb_read_line(path, governor, sizeof(governor)) != 0 || governor[0] == '\0') {
            continue;
        }
        for (i = 0; i < count; i++) {
            known = known || strcmp(seen[i], governor) == 0;
        }
        if (!known && count < 16) {
            snprintf(seen[count], sizeof(seen[count]), "%s", governor);
            count++;
        }
    }
    closedir(dir);

    qsort(seen, (size_t) count, sizeof(seen[0]), bmb_compare_names);
    for (i = 0; i < count; i++) {
        int n = snprintf(m->governor + used, sizeof(m->governor) - used, "%s%s",
                         (i == 0) ? "" : "/", seen[i]);

        /* A governor that does not fit is left out whole, not cut. */
        if (n < 0 || (size_t) n >= sizeof(m->governor) - used) { /* GCOVR_EXCL_BR_LINE: snprintf does not fail on these formats, n < 0 never holds */
            m->governor[used] = '\0';
            break;
        }
        used += (size_t) n;
    }
}

/* intel_pstate has its own switch; acpi-cpufreq and amd-pstate use the
 * generic one. */
static void bmb_probe_turbo(bmb_machine_t *m, const char *root)
{
    char path[512];
    char value[16];

    m->turbo = -1;
    snprintf(path, sizeof(path), "%s/sys/devices/system/cpu/intel_pstate/no_turbo", root);
    if (bmb_read_line(path, value, sizeof(value)) == 0 && (strcmp(value, "0") == 0 || strcmp(value, "1") == 0)) {
        m->turbo = (value[0] == '0');
        m->turbo_control = BMB_TURBO_INTEL_PSTATE;
        return;
    }
    snprintf(path, sizeof(path), "%s/sys/devices/system/cpu/cpufreq/boost", root);
    if (bmb_read_line(path, value, sizeof(value)) == 0 && (strcmp(value, "0") == 0 || strcmp(value, "1") == 0)) {
        m->turbo = (value[0] == '1');
        m->turbo_control = BMB_TURBO_CPUFREQ_BOOST;
    }
}

/* "0-3,8,10-11" is 7 CPUs. 0 when it does not parse. */
static long bmb_count_cpu_list(const char *list)
{
    const char *p = list;
    long count = 0;

    while (*p != '\0') {
        char *end;
        unsigned long first = strtoul(p, &end, 10);
        unsigned long last = first;

        if (end == p || !isdigit((unsigned char) *p)) {
            return 0;
        }
        p = end;
        if (*p == '-') {
            p++;
            last = strtoul(p, &end, 10);
            if (end == p || !isdigit((unsigned char) *p) || last < first) {
                return 0;
            }
            p = end;
        }
        count += (long) (last - first + 1);
        if (*p == ',') {
            p++;
        } else if (*p != '\0') {
            return 0;
        }
    }
    return count;
}

/* cpu.max is "max 100000" (no quota) or "<quota> <period>" in microseconds;
 * a quota of 2.5 periods is 3 CPUs' worth of threads. 0 when there is none. */
static long bmb_read_cpu_max(const char *path)
{
    char line[64];
    char *end;
    unsigned long long quota, period;

    if (bmb_read_line(path, line, sizeof(line)) != 0 || !isdigit((unsigned char) line[0])) {
        return 0;
    }
    quota = strtoull(line, &end, 10);
    if (*end != ' ' || !isdigit((unsigned char) end[1])) {
        return 0;
    }
    period = strtoull(end + 1, &end, 10);
    if (*end != '\0' || quota == 0 || period == 0 || quota / period >= (unsigned long long) LONG_MAX) {
        return 0;
    }
    return (long) (quota / period + (quota % period != 0));
}

/* What threads can actually run at once. The affinity (Cpus_allowed_list)
 * is what taskset, cpusets, Slurm and docker --cpuset-cpus restrict; the
 * cgroup v2 quota (cpu.max) is what docker --cpus and systemd's CPUQuota
 * set. A quota anywhere above the process's cgroup applies to it too, so
 * every level is read, up to the root -- which, in a container with its
 * own cgroup namespace, is the container's cgroup and has a cpu.max. */
static void bmb_probe_usable_cpus(bmb_machine_t *m, const char *root)
{
    char path[512];
    char line[512];
    char cgroup[256] = "";
    FILE *f;
    long usable = 0;

    snprintf(path, sizeof(path), "%s/proc/self/status", root);
    f = fopen(path, "r");
    if (f != NULL) {
        while (fgets(line, sizeof(line), f) != NULL) {
            if (strncmp(line, "Cpus_allowed_list:", 18) == 0) {
                bmb_trim(line + 18);
                usable = bmb_count_cpu_list(line + 18);
                break;
            }
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/proc/self/cgroup", root);
    f = fopen(path, "r");
    if (f != NULL) {
        while (fgets(line, sizeof(line), f) != NULL) {
            if (strncmp(line, "0::/", 4) == 0) {
                /* A path cut short would lead to another cgroup's quota. */
                bmb_trim(line);
                if (strlen(line + 3) < sizeof(cgroup)) {
                    memcpy(cgroup, line + 3, strlen(line + 3) + 1);
                }
                break;
            }
        }
        fclose(f);
    }
    while (cgroup[0] == '/') {
        char *slash;
        long quota;

        snprintf(path, sizeof(path), "%s/sys/fs/cgroup%s/cpu.max", root,
                 (strcmp(cgroup, "/") == 0) ? "" : cgroup);
        quota = bmb_read_cpu_max(path);
        if (quota > 0 && (usable == 0 || quota < usable)) {
            usable = quota;
        }
        if (strcmp(cgroup, "/") == 0) {
            break;
        }
        slash = strrchr(cgroup, '/');
        slash[(slash == cgroup) ? 1 : 0] = '\0';
    }
    m->usable_cpus = usable;
}

void bmb_machine_probe_at(const char *root, bmb_machine_t *m)
{
    struct utsname u;
    time_t now = time(NULL);
    struct tm tm;

    memset(m, 0, sizeof(*m));

    bmb_probe_cpu(m, root);

#ifdef _SC_NPROCESSORS_ONLN
    {
        long n = sysconf(_SC_NPROCESSORS_ONLN);

        m->logical_cpus = (n > 0) ? n : 0;
    }
#endif

    bmb_probe_usable_cpus(m, root);
    bmb_probe_numa(m, root);
    bmb_probe_caches(m, root);
    bmb_probe_governor(m, root);
    bmb_probe_turbo(m, root);

    if (uname(&u) == 0) { /* GCOVR_EXCL_BR_LINE: uname() cannot fail with a valid buffer */
        snprintf(m->os, sizeof(m->os), "%s %s %s", u.sysname, u.release, u.machine);
    }

    if (now != (time_t) -1 && gmtime_r(&now, &tm) != NULL) { /* GCOVR_EXCL_BR_LINE: time() and gmtime_r() of now do not fail */
        strftime(m->date, sizeof(m->date), "%Y-%m-%dT%H:%M:%SZ", &tm);
    }
}

const bmb_machine_t *bmb_machine(void)
{
    static bmb_machine_t machine;
    static int probed = 0;

    if (!probed) {
        /* A test hook (doc/dev/backends.md). CodeQL reads it as path
         * injection; it only lets the user who runs the benchmark make it
         * read, not write, files that user can read already, with no
         * setuid or other boundary crossed, and the alerts are dismissed
         * as used in tests. */
        const char *root = getenv("BMB_MACHINE_ROOT");

        bmb_machine_probe_at((root != NULL) ? root : "", &machine);
        probed = 1;
    }
    return &machine;
}

static void bmb_format_size(unsigned long long bytes, char *buf, size_t size)
{
    if (bytes != 0 && bytes % (1ULL << 30) == 0) {
        snprintf(buf, size, "%lluG", bytes >> 30);
    } else if (bytes != 0 && bytes % (1ULL << 20) == 0) {
        snprintf(buf, size, "%lluM", bytes >> 20);
    } else if (bytes != 0 && bytes % (1ULL << 10) == 0) {
        snprintf(buf, size, "%lluK", bytes >> 10);
    } else {
        snprintf(buf, size, "%lluB", bytes);
    }
}

void bmb_machine_describe_cpu(const bmb_machine_t *m, char *buf, size_t size)
{
    char counts[96] = "";

    if (m->logical_cpus > 0 && m->numa_nodes > 0) {
        snprintf(counts, sizeof(counts), "%ld logical CPU%s, %d NUMA node%s",
                 m->logical_cpus, (m->logical_cpus == 1) ? "" : "s",
                 m->numa_nodes, (m->numa_nodes == 1) ? "" : "s");
    } else if (m->logical_cpus > 0) {
        snprintf(counts, sizeof(counts), "%ld logical CPU%s",
                 m->logical_cpus, (m->logical_cpus == 1) ? "" : "s");
    } else if (m->numa_nodes > 0) {
        snprintf(counts, sizeof(counts), "%d NUMA node%s",
                 m->numa_nodes, (m->numa_nodes == 1) ? "" : "s");
    }

    if (m->cpu[0] != '\0' && counts[0] != '\0') {
        snprintf(buf, size, "%s (%s)", m->cpu, counts);
    } else if (m->cpu[0] != '\0') {
        snprintf(buf, size, "%s", m->cpu);
    } else {
        snprintf(buf, size, "%s", counts);
    }
}

void bmb_machine_describe_caches(const bmb_machine_t *m, char *buf, size_t size)
{
    size_t used = 0;
    int i;

    buf[0] = '\0';
    for (i = 0; i < m->cache_count; i++) {
        const bmb_cache_t *c = &m->caches[i];
        const char *suffix = "";
        char sz[32];
        int n;

        if (strcmp(c->type, "data") == 0) {
            suffix = "d";
        } else if (strcmp(c->type, "instruction") == 0) {
            suffix = "i";
        }
        bmb_format_size(c->size_bytes, sz, sizeof(sz));

        n = snprintf(buf + used, size - used, "%sL%d%s %s",
                     (i == 0) ? "" : ", ", c->level, suffix, sz);
        /* A cache that does not fit is left out whole: cut, "L3 32M"
         * would read as "L3 3". */
        if (n < 0 || (size_t) n >= size - used) { /* GCOVR_EXCL_BR_LINE: snprintf does not fail on these formats, n < 0 never holds */
            buf[used] = '\0';
            return;
        }
        used += (size_t) n;
    }
}

void bmb_machine_describe_frequency(const bmb_machine_t *m, char *buf, size_t size)
{
    const char *turbo = (m->turbo == 1) ? "turbo on" : (m->turbo == 0) ? "turbo off" : "";

    if (m->governor[0] != '\0' && turbo[0] != '\0') {
        snprintf(buf, size, "governor %s, %s", m->governor, turbo);
    } else if (m->governor[0] != '\0') {
        snprintf(buf, size, "governor %s", m->governor);
    } else {
        snprintf(buf, size, "%s", turbo);
    }
}

void bmb_machine_frequency_advice(const bmb_machine_t *m, char *buf, size_t size)
{
    const int scaling = m->governor[0] != '\0' && strcmp(m->governor, "performance") != 0;
    const int turbo = m->turbo == 1;
    const char *turbo_off = (m->turbo_control == BMB_TURBO_INTEL_PSTATE)
        ? "echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo"
        : "echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost";
    char state[192];

    if (!scaling && !turbo) {
        snprintf(buf, size, "%s", "");
        return;
    }
    bmb_machine_describe_frequency(m, state, sizeof(state));
    snprintf(buf, size,
             "The CPU frequency can change during the run (%s), so timings may vary from run "
             "to run. For stable numbers: %s%s%s.",
             state, scaling ? "sudo cpupower frequency-set -g performance" : "",
             (scaling && turbo) ? ", and " : "", turbo ? turbo_off : "");
}
