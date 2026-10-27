/* Checks what the benchmark driver does with a benchmark no routine is:
 * one without a check of its result. --verify, which is on by default,
 * must refuse to run it rather than time numbers nobody checked; with -C
 * it runs. Every real benchmark has its check (test_bmb_verify.sh), so
 * only a stub reaches the refusal. The quartiles, on samples a real run
 * cannot be made to give. And --cache cold (#160), on stubs that count
 * what is done to their copies: as many copies as the last-level cache
 * of the x86 fixture calls for, each call on the next one, each restored
 * after its call and before the next on it -- after --verify's call too,
 * that copy going last -- none called right after its restore, every
 * copy torn down; at most 16384 copies; refused for a benchmark that
 * does not say what its operands take. --align and --ld-pad (#163), on a
 * stub whose setup() asks for an operand and a leading dimension: the
 * operand aligned as asked, the leading dimension padded. And the
 * time-based warm-up and sampling (#164), on stubs whose calls take a
 * known time or none to speak of: --warmup-time once a thread count,
 * --min-time and --max-time in the calls they allow, --max-spread
 * judged on 10 samples at least, or -i, then at each doubling, and never
 * more than 100000, on a clock the test moves itself (#218).
 * And the default size of each level (#165), on stubs that only record
 * the size they are set up for. */

#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bmb_bench.h"
#include "bmb_timer.h"

static int failures;
static int calls;
static volatile double work;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static void *setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    static int ctx;

    (void) dim1;
    (void) dim2;
    (void) thread_count;
    return &ctx;
}

/* Enough work for the clock to see, kept by a volatile the compiler
 * cannot drop (and a newer GCC does not take for unused, as it does a
 * local only written). */
static void call(void *ctx)
{
    int i;

    (void) ctx;
    for (i = 0; i < 1000; i++) {
        work += 1.0;
    }
    calls++;
}

static void teardown(void *ctx)
{
    (void) ctx;
}

/* --cache cold's stubs: a copy is "fresh" once restored, and a call on
 * one that is not counts as stale. */
typedef struct {
    int fresh;
    int calls;
} cold_ctx_t;

static cold_ctx_t *cold_ctxs[32];
static int cold_setups, cold_teardowns, stale_calls, warm_calls;
static const cold_ctx_t *last_reset;

static void *cold_setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    cold_ctx_t *c = malloc(sizeof(*c));

    (void) dim1;
    (void) dim2;
    (void) thread_count;
    if (c != NULL) {
        c->fresh = 1;
        c->calls = 0;
        if (cold_setups < 32) {
            cold_ctxs[cold_setups] = c;
        }
        cold_setups++;
    }
    return c;
}

static void cold_call(void *v)
{
    cold_ctx_t *c = v;
    int i;

    if (!c->fresh) {
        stale_calls++;
    }
    /* Restored and called with no other call in between: the restore
     * brought it back into the cache. */
    if (c == last_reset) {
        warm_calls++;
    }
    last_reset = NULL;
    c->fresh = 0;
    c->calls++;
    for (i = 0; i < 1000; i++) {
        work += 1.0;
    }
}

static void cold_reset(void *v)
{
    ((cold_ctx_t *) v)->fresh = 1;
    last_reset = v;
}

static void cold_teardown(void *v)
{
    free(v);
    cold_teardowns++;
}

/* Counts a copy torn down but keeps it, for round_robin() to read its
 * calls once the run is over; free_kept() frees them then. */
static void keep_teardown(void *v)
{
    (void) v;
    cold_teardowns++;
}

static void free_kept(void)
{
    int i;

    for (i = 0; i < cold_setups && i < 32; i++) {
        free(cold_ctxs[i]);
    }
}

static int cold_verify(void *v, char *msg, size_t size)
{
    (void) msg;
    (void) size;
    /* The check's own call follows its restore, as in a hot run: only the
     * timed calls are held to it. */
    last_reset = NULL;
    cold_call(v);
    return 0;
}

/* 16 MiB: the x86 fixture's 32 MiB L3 then takes 5 copies, the 4 others
 * reaching twice it. */
static double cold_footprint(size_t dim1, size_t dim2)
{
    (void) dim1;
    (void) dim2;
    return 16.0 * 1024 * 1024;
}

/* Twice that, which bytes() may count, an operand read and written. */
static double cold_bytes(size_t dim1, size_t dim2)
{
    return 2.0 * cold_footprint(dim1, dim2);
}

/* A few bytes: more copies than allowed. */
static double tiny_footprint(size_t dim1, size_t dim2)
{
    (void) dim1;
    (void) dim2;
    return 8.0;
}

static void cold_count_reset(void)
{
    cold_setups = cold_teardowns = stale_calls = warm_calls = 0;
    last_reset = NULL;
}

/* The default sizes' stub: the size setup() was given. */
static size_t sized_dim1, sized_dim2;

static void *sized_setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    static int ctx;

    (void) thread_count;
    sized_dim1 = dim1;
    sized_dim2 = dim2;
    return &ctx;
}

/* --align and --ld-pad's stub: what setup() was given. */
static size_t mem_misaligned, mem_ld;

static void *mem_setup(size_t dim1, size_t dim2, unsigned int thread_count)
{
    static int ctx;
    void *operand = bmb_bench_alloc(64);

    (void) dim1;
    (void) dim2;
    (void) thread_count;
    mem_misaligned = (operand == NULL) ? 1 : (size_t) ((uintptr_t) operand % 4096);
    free(operand);
    mem_ld = bmb_bench_ld(4, 5, sizeof(double));
    return &ctx;
}

/* A call of 200 microseconds, spun on the clock: the time-based options'
 * stub, whose calls are counted. With timed_alternate, every other call
 * takes 400: samples that spread for certain, where equal ones can tie
 * to the clock's resolution and spread by nothing (#183); only the first
 * timed_alternate of them, when it is more than 1.
 *
 * With timed_virtual, the call does not spin: it moves the driver's
 * clock by exactly its length. That clock is bmb_timer_now(), which this
 * test defines in place of bmb_timer.c's, linked from libbmbcommon.a
 * only for a program that does not: the real clock, or the virtual one.
 * --max-spread's stop then sees samples no load on the machine can
 * spread; on the real clock, one preempted sample among 10 kept it from
 * stopping there (#218). */
static int timed_calls, timed_alternate, timed_virtual;
static double timed_clock;

static double now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1.0e-9;
}

double bmb_timer_now(void)
{
    return timed_virtual ? timed_clock : now();
}

static void timed_call(void *ctx)
{
    const int longer = timed_alternate && timed_calls % 2 != 0
                       && (timed_alternate == 1 || timed_calls < timed_alternate);
    const double length = longer ? 400.0e-6 : 200.0e-6;
    const double t0 = now();

    (void) ctx;
    timed_calls++;
    if (timed_virtual) {
        timed_clock += length;
        return;
    }
    while (now() - t0 < length) {
        work += 1.0;
    }
}

/* Runs `bench` with -v 8 -x 0 -b 1 -i 1 -C followed by the arguments
 * given, ending in NULL: the calls it made in *count, and the seconds it
 * took in *seconds. */
static int timed_run(const bmb_benchmark_t *bench, int *count, double *seconds, ...)
{
    static char prog[] = "bmb_stub", v[] = "-v", eight[] = "8", x[] = "-x", zero[] = "0",
                b[] = "-b", one[] = "1", i[] = "-i", c[] = "-C";
    char *argv[32] = {prog, v, eight, x, zero, b, one, i, one, c};
    int argc = 10, status;
    double t0;
    va_list ap;
    char *arg;

    va_start(ap, seconds);
    while ((arg = va_arg(ap, char *)) != NULL && argc < 31) {
        argv[argc++] = arg;
    }
    va_end(ap);
    argv[argc] = NULL;
    timed_calls = calls = 0;
    t0 = now();
    status = bmb_benchmark_main(argc, argv, bench);
    *seconds = now() - t0;
    *count = (bench->call == timed_call) ? timed_calls : calls;
    return status;
}

/* Whether the n copies made took `total` calls round robin, from the
 * first: --verify's, then the next copy's. */
static int round_robin(int n, int total)
{
    int i;

    for (i = 0; i < n && i < 32; i++) {
        const int want = total / n + (i < total % n);

        if (cold_ctxs[i]->calls != want) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    static char prog[] = "bmb_stub";
    static char v[] = "-v", one[] = "8", x[] = "-x", zero[] = "0", i[] = "-i", c[] = "-C";
    char *plain[] = {prog, v, one, x, zero, i, one, NULL};
    char *unchecked[] = {prog, v, one, x, zero, i, one, c, NULL};
    bmb_benchmark_t bench = {0};
    char root[512];
    const char *srcdir = getenv("srcdir");

    /* The machine is probed once, on the first run: the x86 fixture's
     * caches, for --cache cold below. */
    snprintf(root, sizeof(root), "%s/fixtures/machine/x86", srcdir ? srcdir : ".");
    setenv("BMB_MACHINE_ROOT", root, 1);

    bench.routine_name = "stub";
    bench.dim1_label = "Vector size";
    bench.use_vector_range = 1;
    bench.setup = setup;
    bench.call = call;
    bench.teardown = teardown;

    check(bmb_benchmark_main(7, plain, &bench) == EXIT_FAILURE && calls == 0,
          "a benchmark without a check is refused under --verify, before any call");
    check(bmb_benchmark_main(8, unchecked, &bench) == EXIT_SUCCESS && calls > 0,
          "with -C it runs");

    /* --cache cold, on the fixture's caches. The stubs' copies are freed
     * by their teardown, so what they record is read before the next
     * run, from counts: the contexts themselves are gone by then. */
    {
        static char k[] = "-k", cold[] = "cold", two[] = "2", seven[] = "7", w[] = "-w";
        char *verified[] = {prog, v, one, x, two, i, seven, k, cold, w, zero, NULL};
        char *bare[] = {prog, v, one, x, zero, i, one, k, cold, c, NULL};
        bmb_benchmark_t cb = {0};
        int status, rr;

        cb.routine_name = "cold stub";
        cb.dim1_label = "Vector size";
        cb.use_vector_range = 1;
        cb.setup = cold_setup;
        cb.call = cold_call;
        cb.reset = cold_reset;
        cb.teardown = keep_teardown;
        cb.verify = cold_verify;
        cb.footprint = cold_footprint;
        cb.reset_every_call = 1;

        /* 2 warm-up calls and 7 samples of one call each, after --verify's. */
        cold_count_reset();
        status = bmb_benchmark_main(11, verified, &cb);
        rr = cold_setups == 5 && round_robin(5, 1 + 2 + 7);
        free_kept();
        check(status == EXIT_SUCCESS && cold_setups == 5, "cold: 5 copies, the others reaching twice the 32 MiB L3");
        check(rr, "cold: each call on the next copy, round robin");
        check(stale_calls == 0, "cold: every call on a restored copy, --verify's one restored after it");
        check(warm_calls == 0, "cold: no call on the copy restored just before, --verify's one going last");

        cb.teardown = cold_teardown;
        cb.footprint = NULL;
        cb.bytes = cold_bytes;
        cold_count_reset();
        status = bmb_benchmark_main(10, bare, &cb);
        check(status == EXIT_SUCCESS && cold_setups == 5 && cold_teardowns == 5,
              "cold: without footprint(), half of bytes(); every copy torn down");

        cb.bytes = NULL;
        cb.footprint = tiny_footprint;
        cold_count_reset();
        status = bmb_benchmark_main(10, bare, &cb);
        check(status == EXIT_SUCCESS && cold_setups == 16384 && cold_teardowns == 16384,
              "cold: operands of a few bytes take 16384 copies, no more");

        cb.footprint = NULL;
        cold_count_reset();
        status = bmb_benchmark_main(10, bare, &cb);
        check(status == EXIT_FAILURE && cold_setups == 0,
              "cold: refused, before any setup, for a benchmark that says nothing of its operands");
    }

    /* --align 4096 and --ld-pad 3 (#163), on a matrix routine's stub. */
    {
        static char a[] = "-a", page[] = "4096", p[] = "-p", three[] = "3", m[] = "-m";
        char *memory[] = {prog, m, one, x, zero, i, one, a, page, p, three, c, NULL};
        bmb_benchmark_t mb = {0};
        int status;

        mb.routine_name = "memory stub";
        mb.dim1_label = "Matrix dim (N)";
        mb.setup = mem_setup;
        mb.call = call;
        mb.teardown = teardown;
        status = bmb_benchmark_main(12, memory, &mb);
        check(status == EXIT_SUCCESS && mem_misaligned == 0, "--align 4096: an operand on a page boundary");
        check(mem_ld == 8, "--ld-pad 3: a leading dimension of 5 + 3");
    }

    /* --warmup-time, --min-time, --max-spread, --max-time (#164). With -b 1,
     * no check and no -x, every call is a warm-up one or a sample. Each
     * timed call takes 200 us at least, so a time allows that many calls
     * at most; the run took that time at least. */
    {
        static char w[] = "-w", wt[] = "0.05", z[] = "0", t[] = "-t", t12[] = "1,2", v2[] = "8,16", T[] = "-T",
                    Tt[] = "0.02", e[] = "-e", loose[] = "50", tight[] = "10", E[] = "-E",
                    Et[] = "0.02", Tbig[] = "3600", i20[] = "20", ibig[] = "100001";
        bmb_benchmark_t tb = {0};
        int n, status;
        double secs;

        tb.routine_name = "timed stub";
        tb.dim1_label = "Vector size";
        tb.use_vector_range = 1;
        tb.setup = setup;
        tb.call = timed_call;
        tb.teardown = teardown;

        status = timed_run(&tb, &n, &secs, w, wt, (char *) NULL);
        check(status == EXIT_SUCCESS && n > 1 && n <= 251 + 1 && secs >= 0.05,
              "--warmup-time 0.05: calls for 0.05 s, then the sample");
        status = timed_run(&tb, &n, &secs, w, wt, t, t12, (char *) NULL);
        check(status == EXIT_SUCCESS && n <= 2 * 251 + 2 && secs >= 0.1,
              "--warmup-time 0.05 -t 1,2: once a thread count");
        status = timed_run(&tb, &n, &secs, w, wt, v, v2, (char *) NULL);
        check(status == EXIT_SUCCESS && n <= 251 + 2 && secs >= 0.05,
              "--warmup-time 0.05 -v 8,16: before the first point only");
        status = timed_run(&tb, &n, &secs, w, z, (char *) NULL);
        check(status == EXIT_SUCCESS && n == 1, "--warmup-time 0: no warm-up, the sample alone");
        status = timed_run(&tb, &n, &secs, w, z, T, Tt, (char *) NULL);
        check(status == EXIT_SUCCESS && n > 1 && n <= 101 && secs >= 0.02,
              "--min-time 0.02: samples until they add up to 0.02 s");
        /* On the virtual clock (#218): calls of exactly 200 us, or 400 and
         * 200 in turn for the first 10, then 200. */
        timed_virtual = 1;
        status = timed_run(&tb, &n, &secs, w, z, e, loose, (char *) NULL);
        check(status == EXIT_SUCCESS && n == 10, "--max-spread 50: calls this regular stop at 10 samples");
        status = timed_run(&tb, &n, &secs, w, z, e, loose, i, i20, (char *) NULL);
        check(status == EXIT_SUCCESS && n == 20, "--max-spread 50 -i 20: at -i, past 10");
        /* 5 of the first 10 at 400 us: a spread of 2/3 of the median.
         * Judged again at 20, with 15 at 200, it is 1/4: met there. */
        timed_alternate = 10;
        status = timed_run(&tb, &n, &secs, w, z, e, loose, (char *) NULL);
        timed_alternate = 0;
        check(status == EXIT_SUCCESS && n == 20,
              "--max-spread 50 not met at 10 samples: judged again at 20, and met there");
        /* Calls of 200 and 400 us in turn spread by 2/3 of their median:
         * --max-spread 10 is never met. On the real clock, the first 10
         * samples of a loaded machine could take the 0.02 s alone. */
        timed_alternate = 1;
        timed_clock = 0.0;
        status = timed_run(&tb, &n, &secs, w, z, e, tight, E, Et, (char *) NULL);
        timed_alternate = 0;
        check(status == EXIT_SUCCESS && n > 10 && n <= 101 && timed_clock >= 0.02,
              "--max-spread never met: --max-time 0.02 stops it");
        timed_virtual = 0;

        /* Calls of a microsecond or so: the cap, and -i past it. */
        tb.call = call;
        status = timed_run(&tb, &n, &secs, w, z, T, Tbig, E, Tbig, (char *) NULL);
        check(status == EXIT_SUCCESS && n == 100000, "--min-time 3600 on short calls: 100000 samples at most");
        status = timed_run(&tb, &n, &secs, w, z, T, Tbig, E, Tbig, i, ibig, (char *) NULL);
        check(status == EXIT_SUCCESS && n == 100001, "and -i 100001, past it, has its samples");
    }

    /* No size given: each level's default (#165); -v and -m still win. */
    {
        static char w[] = "-w", z[] = "0", b[] = "-b", m[] = "-m", m7[] = "7";
        char *plain_args[] = {prog, x, zero, i, one, b, one, w, z, c, NULL};
        char *sized_args[] = {prog, x, zero, i, one, b, one, w, z, c, m, m7, NULL};
        bmb_benchmark_t sb = {0};
        int status;

        sb.routine_name = "sized stub";
        sb.dim1_label = "Vector size";
        sb.setup = sized_setup;
        sb.call = call;
        sb.teardown = teardown;
        sb.use_vector_range = 1;
        sb.level = 1;
        status = bmb_benchmark_main(10, plain_args, &sb);
        check(status == EXIT_SUCCESS && sized_dim1 == 10000000, "level 1: 10000000 by default");
        sb.dim1_label = "Matrix dim (N)";
        sb.use_vector_range = 0;
        sb.level = 2;
        status = bmb_benchmark_main(10, plain_args, &sb);
        check(status == EXIT_SUCCESS && sized_dim1 == 5000, "level 2: 5000 by default");
        sb.level = 3;
        sb.dim2_label = "Matrix dim2 (N)";
        status = bmb_benchmark_main(10, plain_args, &sb);
        check(status == EXIT_SUCCESS && sized_dim1 == 2000 && sized_dim2 == 2000,
              "level 3: 2000 by default, square");
        status = bmb_benchmark_main(12, sized_args, &sb);
        check(status == EXIT_SUCCESS && sized_dim1 == 7, "level 3 with -m 7: 7");
    }

    /* The values R's quantile() gives, exactly representable. One sample
     * is both quartiles; the NaN after it must never be read. */
    {
        static const double one_sample[] = {1.0, NAN};
        static const double two[] = {1.0, 2.0};
        static const double four[] = {1.0, 2.0, 3.0, 4.0};
        static const double five[] = {1.0, 2.0, 4.0, 8.0, 16.0};
        double q1, q3;

        bmb_quartiles(one_sample, 1, &q1, &q3);
        check(q1 == 1.0 && q3 == 1.0, "quartiles of one sample: that sample, nothing past it read");
        bmb_quartiles(two, 2, &q1, &q3);
        check(q1 == 1.25 && q3 == 1.75, "quartiles of two samples: a quarter of the way in from either end");
        bmb_quartiles(four, 4, &q1, &q3);
        check(q1 == 1.75 && q3 == 3.25, "quartiles of four samples, as R gives them");
        bmb_quartiles(five, 5, &q1, &q3);
        check(q1 == 2.0 && q3 == 8.0, "quartiles of five samples: on samples themselves");
    }

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
