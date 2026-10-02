#ifndef BMB_MACHINE_H
#define BMB_MACHINE_H

#include <stddef.h>

/* What the run happened on, recorded next to the results so that two files
 * from different machines are never mistaken for one series (#1, decisions
 * 1 and 4). Deliberately *not* the hostname: reports get shared, and on a
 * cluster identical nodes have different names.
 *
 * Every field is best effort. One that cannot be read is left empty (or 0)
 * and omitted from the output, the way `blas` is when the library exposes
 * no version string -- never guessed. */

#define BMB_MACHINE_MAX_CACHES 8

typedef struct {
    int level;
    char type[16];                 /* "data", "instruction" or "unified" */
    unsigned long long size_bytes;
} bmb_cache_t;

typedef struct {
    char cpu[256];                 /* model name; "" when unknown */
    long logical_cpus;             /* 0 when unknown */
    int numa_nodes;                /* 0 when unknown */
    bmb_cache_t caches[BMB_MACHINE_MAX_CACHES];
    int cache_count;
    char os[256];                  /* "Linux 6.6.87 x86_64"; "" when unknown */
    char date[32];                 /* UTC, ISO 8601; "" when unknown */
} bmb_machine_t;

/* Probed once, on the first call. The date is the time of that call, which
 * in a benchmark is when the run started. */
const bmb_machine_t *bmb_machine(void);

/* "Intel(R) Core(TM) Ultra 7 155U (14 logical CPUs, 1 NUMA node)", or as
 * much of it as is known; "" when nothing is. */
void bmb_machine_describe_cpu(const bmb_machine_t *m, char *buf, size_t size);

/* "L1d 48K, L1i 64K, L2 2M, L3 12M"; "" when no cache could be read. */
void bmb_machine_describe_caches(const bmb_machine_t *m, char *buf, size_t size);

#endif /* BMB_MACHINE_H */
