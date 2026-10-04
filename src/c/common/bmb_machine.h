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

/* Where turbo is switched, which is also what to tell a user to write to. */
typedef enum {
    BMB_TURBO_UNKNOWN = 0,
    BMB_TURBO_INTEL_PSTATE,        /* intel_pstate/no_turbo: 1 is off */
    BMB_TURBO_CPUFREQ_BOOST        /* cpufreq/boost: 0 is off */
} bmb_turbo_control_t;

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
    char governor[128];            /* cpufreq governor, or the distinct ones
                                    * sorted, "performance/powersave", when
                                    * CPUs differ; "" when unknown */
    int turbo;                     /* 1 on, 0 off, -1 unknown */
    bmb_turbo_control_t turbo_control;
    char os[256];                  /* "Linux 6.6.87 x86_64"; "" when unknown */
    char date[32];                 /* UTC, ISO 8601; "" when unknown */
} bmb_machine_t;

/* Probed once, on the first call. The date is the time of that call, which
 * in a benchmark is when the run started. BMB_MACHINE_ROOT, when set, is
 * the root it probes (see bmb_machine_probe_at): a test hook, so that the
 * benchmarks' output can be checked against the fixtures. */
const bmb_machine_t *bmb_machine(void);

/* Probes as though procfs and sysfs were mounted under `root` ("" for the
 * real ones), which is what bmb_machine() does with "". It exists so the
 * paths that only occur elsewhere -- an aarch64 /proc/cpuinfo with no model
 * name, a container without sysfs -- can be tested on any machine. The
 * logical CPU count, the OS and the date still come from the real system. */
void bmb_machine_probe_at(const char *root, bmb_machine_t *m);

/* "Intel(R) Core(TM) Ultra 7 155U (14 logical CPUs, 1 NUMA node)", or as
 * much of it as is known; "" when nothing is. */
void bmb_machine_describe_cpu(const bmb_machine_t *m, char *buf, size_t size);

/* "L1d 48K, L1i 64K, L2 2M, L3 12M"; "" when no cache could be read. */
void bmb_machine_describe_caches(const bmb_machine_t *m, char *buf, size_t size);

/* "governor performance, turbo off", or as much of it as is known; "" when
 * nothing is. */
void bmb_machine_describe_frequency(const bmb_machine_t *m, char *buf, size_t size);

/* What to say when the CPU frequency can change during a run -- a governor
 * other than performance, or turbo on -- with the commands that stop it;
 * "" when it cannot, or when nothing is known. */
void bmb_machine_frequency_advice(const bmb_machine_t *m, char *buf, size_t size);

#endif /* BMB_MACHINE_H */
