#ifndef BMB_LOG_H
#define BMB_LOG_H

#include <stdio.h>

/* Results go to stdout; everything here is diagnostics and goes to
 * stderr, so that `bmb_dgemm > results.txt` keeps the two apart. Each
 * kind has its colour on a terminal, none when stderr is not one or when
 * NO_COLOR is set in the environment. */
void bmb_log_debug(const char *message);
void bmb_log_warning(const char *message);
void bmb_log_error(const char *message);

#endif /* BMB_LOG_H */
