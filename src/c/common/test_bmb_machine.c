/* Checks the machine probe.
 *
 * Every field it fills is best effort by design: a container without sysfs
 * cache information, or an aarch64 kernel without a "model name" line, is a
 * legitimate "unknown", not a failure. So this checks only what any Linux
 * box guarantees (uname, sysconf, the clock), and that whatever *was*
 * recorded is sane -- a cache of size 0 or level 9 would be a parsing bug. */

#include <stdio.h>
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

int main(void)
{
    const bmb_machine_t *m = bmb_machine();
    char buf[512];
    int i;

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
