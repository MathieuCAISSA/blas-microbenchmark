#define _GNU_SOURCE

#include <config.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)
#include <sched.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#endif

#include "bmb_bench.h"
#include "bmb_log.h"
#include "bmb_star.h"

/* Marks the CPUs of a Linux CPU list ("0-3,8") below max in set. 0, or
 * -1 when it is not one. */
static int bmb_star_parse_list(const char *list, unsigned char *set, int max)
{
    const char *p = list;

    while (*p != '\0' && *p != '\n') {
        char *end;
        long first = strtol(p, &end, 10);
        long last = first;
        long c;

        if (end == p || first < 0) {
            return -1;
        }
        if (*end == '-') {
            p = end + 1;
            last = strtol(p, &end, 10);
            if (end == p || last < first) {
                return -1;
            }
        }
        for (c = first; c <= last && c < max; c++) {
            set[c] = 1;
        }
        p = end;
        if (*p == ',') {
            p++;
        } else if (*p != '\0' && *p != '\n') {
            return -1;
        }
    }
    return 0;
}

/* Marks the CPUs of cpu's core in set: those that share it, under
 * core_cpus_list, or thread_siblings_list before Linux 5.6. 0, or -1
 * when neither can be read. */
static int bmb_star_core_of(const char *root, int cpu, unsigned char *set, int max)
{
    static const char *const names[] = {"core_cpus_list", "thread_siblings_list"};
    char path[512];
    char line[1024];
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        FILE *f;
        int ok;

        snprintf(path, sizeof(path), "%s/sys/devices/system/cpu/cpu%d/topology/%s", root, cpu,
                 names[i]);
        f = fopen(path, "r");
        if (f == NULL) {
            continue;
        }
        ok = fgets(line, sizeof(line), f) != NULL && bmb_star_parse_list(line, set, max) == 0;
        fclose(f);
        if (ok) {
            return 0;
        }
    }
    return -1;
}

int bmb_star_cpus_at(const char *root, const char *allowed, int *cpus, int max)
{
    unsigned char in[BMB_STAR_MAX] = {0};
    unsigned char chosen[BMB_STAR_MAX] = {0}; /* 2 chosen, 1 on a chosen one's core */
    int count = 0;
    int c;

    if (allowed != NULL) {
        if (bmb_star_parse_list(allowed, in, BMB_STAR_MAX) != 0) {
            return 0;
        }
    } else {
#if defined(__linux__)
        cpu_set_t set;

        if (sched_getaffinity(0, sizeof(set), &set) != 0) { /* GCOVR_EXCL_BR_LINE: the calling process's own affinity can always be read */
            return 0; /* GCOVR_EXCL_LINE: the calling process's own affinity can always be read */
        }
        for (c = 0; c < BMB_STAR_MAX && c < CPU_SETSIZE; c++) {
            in[c] = CPU_ISSET(c, &set) ? 1 : 0;
        }
#else
        return 0;
#endif
    }

    /* The first CPU of each core: two instances on the threads of one core
     * would share its units. A CPU is passed over when its core holds one
     * already chosen, whichever of the two says so. */
    for (c = 0; c < BMB_STAR_MAX && count < max; c++) {
        unsigned char core[BMB_STAR_MAX] = {0};
        int o;
        int shared = 0;

        if (!in[c] || chosen[c]) {
            continue;
        }
        (void) bmb_star_core_of(root, c, core, BMB_STAR_MAX);
        for (o = 0; o < BMB_STAR_MAX && !shared; o++) {
            shared = core[o] && chosen[o] == 2;
        }
        if (shared) {
            continue;
        }
        cpus[count++] = c;
        for (o = 0; o < BMB_STAR_MAX; o++) {
            if (core[o] && chosen[o] == 0) {
                chosen[o] = 1;
            }
        }
        chosen[c] = 2;
    }
    return count;
}

void bmb_star_describe(const int *cpus, int count, char *buf, size_t size)
{
    size_t at = 0;
    int i = 0;

    if (size == 0) {
        return;
    }
    buf[0] = '\0';
    while (i < count) {
        int j = i;
        int w;

        while (j + 1 < count && cpus[j + 1] == cpus[j] + 1) {
            j++;
        }
        if (j == i) {
            w = snprintf(buf + at, size - at, "%s%d", i == 0 ? "" : ",", cpus[i]);
        } else {
            w = snprintf(buf + at, size - at, "%s%d%s%d", i == 0 ? "" : ",", cpus[i],
                         j == i + 1 ? "," : "-", cpus[j]);
        }
        if (w < 0 || (size_t) w >= size - at) {
            buf[at] = '\0';
            return;
        }
        at += (size_t) w;
        i = j + 1;
    }
}

#if defined(__linux__)
/* What the instances share: a barrier, and a value each for
 * bmb_star_max(). In memory mapped before fork(), so shared. */
typedef struct {
    atomic_uint arrived;
    atomic_uint generation;
    atomic_uint finished;   /* the instances past bmb_star_finish()'s barrier */
    double slot[];
} bmb_star_shared_t;

static bmb_star_shared_t *bmb_star_shm;
static size_t bmb_star_shm_size;
static unsigned int bmb_star_count;
static int bmb_star_index = -1;
static int bmb_star_pipe[2] = {-1, -1};
static pid_t *bmb_star_pids;
static const int *bmb_star_on;

int bmb_star_instance(void)
{
    return bmb_star_index;
}

int bmb_star_cpu(void)
{
    return bmb_star_on[bmb_star_index];
}

/* Every instance waits there until all have come. Spinning, not
 * sleeping: each has a core of its own, and one woken by the kernel would
 * start its next sample tens of microseconds after the others. The
 * generation is read before arriving, so that the last one's increment
 * is never missed. */
static void bmb_star_barrier(void)
{
    const unsigned int gen = atomic_load_explicit(&bmb_star_shm->generation, memory_order_acquire);

    if (atomic_fetch_add_explicit(&bmb_star_shm->arrived, 1, memory_order_acq_rel) + 1 == bmb_star_count) {
        atomic_store_explicit(&bmb_star_shm->arrived, 0, memory_order_relaxed);
        atomic_fetch_add_explicit(&bmb_star_shm->generation, 1, memory_order_release);
        return;
    }
    while (atomic_load_explicit(&bmb_star_shm->generation, memory_order_acquire) == gen) {
    }
}

double bmb_star_max(double v)
{
    double most;
    unsigned int i;

    if (bmb_star_index < 0) {
        return v;
    }
    bmb_star_shm->slot[bmb_star_index] = v;
    bmb_star_barrier();
    most = bmb_star_shm->slot[0];
    for (i = 1; i < bmb_star_count; i++) {
        if (bmb_star_shm->slot[i] > most) {
            most = bmb_star_shm->slot[i];
        }
    }
    /* None writes its slot again before all have read them. */
    bmb_star_barrier();
    return most;
}

void bmb_star_finish(void)
{
    bmb_star_barrier();
    atomic_fetch_add_explicit(&bmb_star_shm->finished, 1, memory_order_release);
}

int bmb_star_send(const bmb_result_row_t *row)
{
    const char *p = (const char *) row;
    size_t left = sizeof(*row);

    while (left > 0) {
        ssize_t n = write(bmb_star_pipe[1], p, left);

        if (n < 0 && errno == EINTR) { /* GCOVR_EXCL_BR_LINE: no signal is caught */
            continue; /* GCOVR_EXCL_LINE: no signal is caught */
        }
        if (n <= 0) { /* GCOVR_EXCL_BR_LINE: the calling process reads until the instances end */
            bmb_log_error("--star: a result could not be passed on from the instances."); /* GCOVR_EXCL_LINE: the calling process reads until the instances end */
            return -1; /* GCOVR_EXCL_LINE: the calling process reads until the instances end */
        }
        p += n;
        left -= (size_t) n;
    }
    return 0;
}
#else
/* Without Linux's affinity no instance is started (bmb_star_start), so
 * this process is never one. */
int bmb_star_instance(void)
{
    return -1;
}

int bmb_star_cpu(void)
{
    return -1;
}

double bmb_star_max(double v)
{
    return v;
}

void bmb_star_finish(void)
{
}

int bmb_star_send(const bmb_result_row_t *row)
{
    (void) row;
    return -1;
}
#endif

int bmb_star_exit_code(int status)
{
    if (status == -2) {
        return BMB_EXIT_WRONG_RESULT;
    }
    return (status == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#if defined(__linux__)
static void bmb_star_release(void)
{
    if (bmb_star_shm != NULL) {
        munmap(bmb_star_shm, bmb_star_shm_size);
    }
    bmb_star_shm = NULL;
    if (bmb_star_pipe[0] >= 0) {
        close(bmb_star_pipe[0]);
    }
    if (bmb_star_pipe[1] >= 0) { /* GCOVR_EXCL_BR_LINE: open only after a start that failed, out of memory, descriptors or processes */
        close(bmb_star_pipe[1]); /* GCOVR_EXCL_LINE: open only after a start that failed, out of memory, descriptors or processes */
    }
    bmb_star_pipe[0] = bmb_star_pipe[1] = -1;
    free(bmb_star_pids);
    bmb_star_pids = NULL;
}
#endif

#if defined(__linux__)
/* Stops the instances still running: one that is waiting at a barrier for
 * one that is gone would wait forever. */
static void bmb_star_stop(void)
{
    unsigned int i;

    for (i = 0; i < bmb_star_count; i++) {
        if (bmb_star_pids[i] > 0) {
            kill(bmb_star_pids[i], SIGKILL);
        }
    }
}

/* In instance i, just started: tied to the calling process, so that it
 * ends with it, and pinned. */
static int bmb_star_enter(unsigned int i, pid_t parent)
{
    cpu_set_t set;
    char msg[128];

    bmb_star_index = (int) i;
    close(bmb_star_pipe[0]);
    bmb_star_pipe[0] = -1;
    free(bmb_star_pids);
    bmb_star_pids = NULL;
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() != parent) { /* GCOVR_EXCL_BR_LINE: only when the calling process died during fork() */
        _exit(EXIT_FAILURE); /* GCOVR_EXCL_LINE: only when the calling process died during fork() */
    }
    CPU_ZERO(&set);
    CPU_SET(bmb_star_on[i], &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) { /* GCOVR_EXCL_BR_LINE: the CPUs come from this process's own affinity */
        snprintf(msg, sizeof(msg), "--star: instance %u could not be pinned to CPU %d.", i, bmb_star_on[i]); /* GCOVR_EXCL_LINE: the CPUs come from this process's own affinity */
        bmb_log_error(msg); /* GCOVR_EXCL_LINE: the CPUs come from this process's own affinity */
        _exit(EXIT_FAILURE); /* GCOVR_EXCL_LINE: the CPUs come from this process's own affinity */
    }
    return (int) i;
}
#endif

int bmb_star_start(unsigned int instances, const int *cpus)
{
#if defined(__linux__)
    const pid_t parent = getpid();
    unsigned int i;

    bmb_star_count = instances;
    bmb_star_on = cpus;
    bmb_star_pids = calloc(instances, sizeof(*bmb_star_pids));
    if (bmb_star_pids == NULL) {
        bmb_log_error("--star: out of memory while starting the instances.");
        return -2;
    }
    bmb_star_shm_size = sizeof(*bmb_star_shm) + instances * sizeof(double);
    bmb_star_shm = mmap(NULL, bmb_star_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (bmb_star_shm == MAP_FAILED || pipe(bmb_star_pipe) != 0) { /* GCOVR_EXCL_BR_LINE: a page or a pipe, only out of memory or file descriptors */
        if (bmb_star_shm == MAP_FAILED) { /* GCOVR_EXCL_BR_LINE: out of memory */
            bmb_star_shm = NULL; /* GCOVR_EXCL_LINE: out of memory */
        }
        bmb_log_error("--star: the instances could not be set up (out of memory or file descriptors)."); /* GCOVR_EXCL_LINE: a page or a pipe, only out of memory or file descriptors */
        bmb_star_release(); /* GCOVR_EXCL_LINE: a page or a pipe, only out of memory or file descriptors */
        return -2; /* GCOVR_EXCL_LINE: a page or a pipe, only out of memory or file descriptors */
    }
    atomic_init(&bmb_star_shm->arrived, 0);
    atomic_init(&bmb_star_shm->generation, 0);
    atomic_init(&bmb_star_shm->finished, 0);

    for (i = 0; i < instances; i++) {
        const pid_t pid = fork();

        if (pid == 0) {
            return bmb_star_enter(i, parent);
        }
        if (pid < 0) { /* GCOVR_EXCL_BR_LINE: only past the process limit */
            bmb_log_error("--star: an instance could not be started (fork failed)."); /* GCOVR_EXCL_LINE: only past the process limit */
            bmb_star_stop(); /* GCOVR_EXCL_LINE: only past the process limit */
            while (wait(NULL) > 0) { /* GCOVR_EXCL_LINE: only past the process limit */
            } /* GCOVR_EXCL_LINE: only past the process limit */
            bmb_star_release(); /* GCOVR_EXCL_LINE: only past the process limit */
            return -2; /* GCOVR_EXCL_LINE: only past the process limit */
        }
        bmb_star_pids[i] = pid;
    }
    close(bmb_star_pipe[1]);
    bmb_star_pipe[1] = -1;
    return -1;
#else
    (void) instances;
    (void) cpus;
    bmb_log_error("--star: pinning an instance to a core is done with Linux's sched_setaffinity, "
                  "which this system does not have.");
    return -2;
#endif
}

#if defined(__linux__)
/* The status of an instance that ended: -1 when it failed, -2 when it
 * found a wrong result, 1 when it ended before the others had all come
 * to the end, which then wait for it forever. That is told, unless it is
 * the calling process that stopped it. */
static int bmb_star_ended(unsigned int i, int wstatus, int stopping)
{
    char msg[160];

    if (WIFEXITED(wstatus) && atomic_load_explicit(&bmb_star_shm->finished, memory_order_acquire) > 0) {
        switch (WEXITSTATUS(wstatus)) {
        case EXIT_SUCCESS:
            return 0;
        case BMB_EXIT_WRONG_RESULT:
            return -2;
        default:
            return -1;
        }
    }
    if (!stopping) {
        if (WIFSIGNALED(wstatus)) { /* GCOVR_EXCL_BR_LINE: an instance that ends before the others without a signal, only by an exit() in the library */
            snprintf(msg, sizeof(msg), "--star: instance %u, on CPU %d, was killed by signal %d.",
                     i, bmb_star_on[i], WTERMSIG(wstatus));
        } else {
            /* GCOVR_EXCL_START: only an exit() in the library ends an instance before the others without a signal */
            snprintf(msg, sizeof(msg), "--star: instance %u, on CPU %d, ended before the others.",
                     i, bmb_star_on[i]);
            /* GCOVR_EXCL_STOP */
        }
        bmb_log_error(msg);
    }
    return 1;
}
#endif

int bmb_star_wait(int (*on_row)(const bmb_result_row_t *row, void *arg), void *arg)
{
#if defined(__linux__)
    bmb_result_row_t row;
    size_t got = 0;
    unsigned int live = bmb_star_count;
    int reading = 1;
    int stopping = 0;
    int status = 0;

    while (reading || live > 0) {
        unsigned int i;

        if (reading) {
            struct pollfd pfd;

            pfd.fd = bmb_star_pipe[0];
            pfd.events = POLLIN;
            pfd.revents = 0;
            if (poll(&pfd, 1, 100) > 0) {
                ssize_t n = read(bmb_star_pipe[0], (char *) &row + got, sizeof(row) - got);

                if (n > 0) {
                    got += (size_t) n;
                    if (got == sizeof(row)) {
                        got = 0;
                        if (on_row(&row, arg) != 0 && status == 0) { /* GCOVR_EXCL_BR_LINE: on_row fails only out of memory, excluded where it does */
                            status = -1; /* GCOVR_EXCL_LINE: on_row fails only out of memory, excluded where it does */
                        }
                    }
                } else if (n == 0 || errno != EINTR) { /* GCOVR_EXCL_BR_LINE: no signal is caught */
                    reading = 0;
                }
            }
        }

        /* Waited for without blocking while there may be rows to read; one
         * lost before the end stops the others. */
        for (i = 0; i < bmb_star_count; i++) {
            int wstatus;
            int s;

            if (bmb_star_pids[i] <= 0 || waitpid(bmb_star_pids[i], &wstatus, reading ? WNOHANG : 0) <= 0) {
                continue;
            }
            bmb_star_pids[i] = 0;
            live--;
            s = bmb_star_ended(i, wstatus, stopping);
            if (s == 1) {
                if (!stopping) {
                    stopping = 1;
                    bmb_star_stop();
                }
                s = -1;
            }
            if (s == -2 || (s == -1 && status == 0)) {
                status = s;
            }
        }
    }
    bmb_star_release();
    return status;
#else
    (void) on_row;
    (void) arg;
    return -1;
#endif
}
