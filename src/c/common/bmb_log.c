#include <stdio.h>

#include "bmb_log.h"

#define BMB_COLOR_RESET   "\033[0m"
#define BMB_COLOR_DEBUG   "\033[36m" /* cyan */
#define BMB_COLOR_WARNING "\033[33m" /* yellow */
#define BMB_COLOR_ERROR   "\033[31m" /* red */

void bmb_log_format(const char *message, const char *color)
{
    if (color != NULL) {
        fprintf(stdout, "%s%s%s\n", color, message, BMB_COLOR_RESET);
    } else {
        fprintf(stdout, "%s\n", message);
    }
}

void bmb_log_info(const char *message)
{
    bmb_log_format(message, NULL);
}

void bmb_log_debug(const char *message)
{
#ifdef BMB_DEBUG
    bmb_log_format(message, BMB_COLOR_DEBUG);
#else
    (void) message;
#endif
}

void bmb_log_warning(const char *message)
{
    bmb_log_format(message, BMB_COLOR_WARNING);
}

void bmb_log_error(const char *message)
{
    bmb_log_format(message, BMB_COLOR_ERROR);
}
