#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bmb_log.h"

#define BMB_COLOR_RESET   "\033[0m"
#define BMB_COLOR_DEBUG   "\033[36m" /* cyan */
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

void bmb_log_format(FILE *stream, const char *message, const char *color)
{
    if (color != NULL && bmb_log_use_color(stream)) {
        fprintf(stream, "%s%s%s\n", color, message, BMB_COLOR_RESET);
    } else {
        fprintf(stream, "%s\n", message);
    }
}

void bmb_log_info(const char *message)
{
    bmb_log_format(stdout, message, NULL);
}

void bmb_log_debug(const char *message)
{
#ifdef BMB_DEBUG
    bmb_log_format(stderr, message, BMB_COLOR_DEBUG);
#else
    (void) message;
#endif
}

void bmb_log_warning(const char *message)
{
    bmb_log_format(stderr, message, BMB_COLOR_WARNING);
}

void bmb_log_error(const char *message)
{
    bmb_log_format(stderr, message, BMB_COLOR_ERROR);
}
