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
 * does not say what its operands take. And --align and --ld-pad (#163),
 * on a stub whose setup() asks for an operand and a leading dimension:
 * the operand aligned as asked, the leading dimension padded. */

#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"

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
        static char k[] = "-k", cold[] = "cold", two[] = "2", seven[] = "7";
        char *verified[] = {prog, v, one, x, two, i, seven, k, cold, NULL};
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
        status = bmb_benchmark_main(9, verified, &cb);
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
