#define _POSIX_C_SOURCE 200809L

#include <time.h>

#include "bmb_timer.h"

double bmb_timer_now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}
