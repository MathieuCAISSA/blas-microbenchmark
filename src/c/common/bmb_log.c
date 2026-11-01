#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bmb_log.h"

#define BMB_COLOR_RESET   "\033[0m"
#define BMB_COLOR_WARNING "\033[33m" /* yellow */
#define BMB_COLOR_ERROR   "\033[31m" /* red */

/* Escape sequences are for humans at a terminal: emitting them into a
 * redirected file would corrupt the very results the user is collecting. */
static int bmb_log_use_color(FILE *stream)
{
    const char *no_color = getenv("NO_COLOR");

    if (no_color != NULL && no_color[0] != '\0') {
        return 0;
    }

    return isatty(fileno(stream));
}

static int bmb_log_holding;
static char bmb_log_held[4096];
static size_t bmb_log_held_len;

/* A message is written whole, in one call: under --star, several
 * processes share stderr, and a line written in pieces could be cut by
 * another's. */
static void bmb_log_format(FILE *stream, const char *message, const char *color)
{
    char line[1200];

    if (bmb_log_use_color(stream)) {
        snprintf(line, sizeof(line), "%s%s%s\n", color, message, BMB_COLOR_RESET);
    } else {
        snprintf(line, sizeof(line), "%s\n", message);
    }
    if (bmb_log_holding) {
        const size_t n = strlen(line);

        if (bmb_log_held_len + n < sizeof(bmb_log_held)) {
            memcpy(bmb_log_held + bmb_log_held_len, line, n + 1);
            bmb_log_held_len += n;
        }
        return;
    }
    fputs(line, stream);
}

void bmb_log_hold(void)
{
    bmb_log_holding = 1;
    bmb_log_held_len = 0;
    bmb_log_held[0] = '\0';
}

void bmb_log_release(int write)
{
    bmb_log_holding = 0;
    if (write && bmb_log_held_len > 0) {
        fputs(bmb_log_held, stderr);
    }
    bmb_log_held_len = 0;
}

void bmb_log_warning(const char *message)
{
    bmb_log_format(stderr, message, BMB_COLOR_WARNING);
}

void bmb_log_error(const char *message)
{
    bmb_log_format(stderr, message, BMB_COLOR_ERROR);
}
