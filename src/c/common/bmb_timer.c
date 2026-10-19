#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <time.h>

#include "bmb_log.h"
#include "bmb_timer.h"

double bmb_timer_now(void)
{
    struct timespec ts;

    /* Fatal rather than swallowed: there is no sensible value to return
     * from a clock that failed, and a benchmark whose timer does not work
     * has nothing to report. On Linux CLOCK_MONOTONIC cannot fail with a
     * valid pointer, so this only fires on a platform that does not
     * support it -- where every number this program could print would be
     * meaningless anyway. */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) { /* GCOVR_EXCL_BR_LINE: CLOCK_MONOTONIC cannot fail on Linux, as above */
        /* GCOVR_EXCL_START: CLOCK_MONOTONIC cannot fail on Linux, as above */
        bmb_log_error("clock_gettime(CLOCK_MONOTONIC) failed; cannot measure anything.");
        exit(EXIT_FAILURE);
        /* GCOVR_EXCL_STOP */
    }

    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}
