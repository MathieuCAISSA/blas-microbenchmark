#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
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

    bmb_probe_numa(m, root);
    bmb_probe_caches(m, root);

    if (uname(&u) == 0) {
        snprintf(m->os, sizeof(m->os), "%s %s %s", u.sysname, u.release, u.machine);
    }

    if (now != (time_t) -1 && gmtime_r(&now, &tm) != NULL) {
        strftime(m->date, sizeof(m->date), "%Y-%m-%dT%H:%M:%SZ", &tm);
    }
}

const bmb_machine_t *bmb_machine(void)
{
    static bmb_machine_t machine;
    static int probed = 0;

    if (!probed) {
        bmb_machine_probe_at("", &machine);
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
        if (n < 0 || (size_t) n >= size - used) {
            return;
        }
        used += (size_t) n;
    }
}
