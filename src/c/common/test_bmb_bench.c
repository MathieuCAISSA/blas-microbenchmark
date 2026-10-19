/* Checks what the benchmark driver does with a benchmark no routine is:
 * one without a check of its result. --verify, which is on by default,
 * must refuse to run it rather than time numbers nobody checked; with -C
 * it runs. Every real benchmark has its check (test_bmb_verify.sh), so
 * only a stub reaches the refusal. */

#include <stdio.h>
#include <stdlib.h>

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

int main(void)
{
    static char prog[] = "bmb_stub";
    static char v[] = "-v", one[] = "8", x[] = "-x", zero[] = "0", i[] = "-i", c[] = "-C";
    char *plain[] = {prog, v, one, x, zero, i, one, NULL};
    char *unchecked[] = {prog, v, one, x, zero, i, one, c, NULL};
    bmb_benchmark_t bench = {0};

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

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
