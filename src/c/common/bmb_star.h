#ifndef BMB_STAR_H
#define BMB_STAR_H

#include <stddef.h>

#include "bmb_result.h"

/* --star (#173): N copies of the benchmark, one thread each, each a
 * process pinned to a core of its own, as N MPI ranks or the jobs of a
 * task farm run, HPC Challenge's "Star" mode. Processes rather than
 * threads: each has its own operands, touched first on its own core, and
 * no instance waits on a lock the library takes for another's call.
 *
 * The instances agree at every step that decides what is measured: a
 * point is skipped by all when one could not set it up, a wrong result in
 * one ends the run in all, the batch is the largest any chose. Each
 * sample starts in all at once and lasts until the slowest is done; its
 * time is the slowest's, which every instance holds, so that they all
 * take the same number of samples. */

/* The most instances: as many as cpu_set_t has CPUs. */
#define BMB_STAR_MAX 1024

/* The CPUs to pin the instances to, one per core: of the CPUs this
 * process may run on, the first of each core, in order. The cores are
 * read from <root>/sys/devices/system/cpu/cpu<N>/topology ("" for the
 * real sysfs, a fixture's root in the tests); a CPU whose core cannot be
 * read is a core of its own. `allowed` is the affinity, as Linux lists
 * it ("0-3,8"); NULL reads this process's. The count, 0 when none could
 * be found. */
int bmb_star_cpus_at(const char *root, const char *allowed, int *cpus, int max);

/* Starts `instances` instances on cpus[0..instances-1] and returns, in
 * each, its index; in the calling process, which measures nothing, -1
 * once they all run, or -2 when they could not be started (logged).
 * Before it the caller flushes what it wrote, which the instances would
 * write again. */
int bmb_star_start(unsigned int instances, const int *cpus);

/* In an instance, its index; -1 outside --star. */
int bmb_star_instance(void);

/* In an instance, the CPU it is pinned to. */
int bmb_star_cpu(void);

/* The largest of the values every instance passes, returned in all once
 * they all have: what they agree on. Outside --star, v. */
double bmb_star_max(double v);

/* In instance 0, a row to send to the calling process. 0, or -1 when it
 * could not be (logged). */
int bmb_star_send(const bmb_result_row_t *row);

/* In the calling process: each row instance 0 sends, given to on_row as
 * it comes, until the instances end. Their status: 0, -1 when one failed
 * or was lost (killed, or ended before the others, which are then
 * stopped), -2 when one found a wrong result. */
int bmb_star_wait(int (*on_row)(const bmb_result_row_t *row, void *arg), void *arg);

/* In an instance, once it has measured all it had to: waits for the
 * others to have too. One that ends before they all have is taken as
 * lost by bmb_star_wait(). */
void bmb_star_finish(void);

/* In an instance, at its end: its status as an exit code. */
int bmb_star_exit_code(int status);

#endif /* BMB_STAR_H */
