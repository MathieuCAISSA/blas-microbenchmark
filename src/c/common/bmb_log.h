#ifndef BMB_LOG_H
#define BMB_LOG_H

#include <stdio.h>

/* Prints message to stream, wrapped in the given ANSI color escape code
 * (e.g. "\033[33m"), or with no color if color is NULL. The color is also
 * dropped when stream is not a terminal, or when NO_COLOR is set in the
 * environment. */
void bmb_log_format(FILE *stream, const char *message, const char *color);

/* Results go to stdout; everything below is diagnostics and goes to
 * stderr, so that `bmb_dgemm > results.txt` keeps the two apart. */
void bmb_log_info(const char *message);
void bmb_log_debug(const char *message);
void bmb_log_warning(const char *message);
void bmb_log_error(const char *message);

#endif /* BMB_LOG_H */
