#ifndef BMB_LOG_H
#define BMB_LOG_H

#include <stdio.h>

/* Results go to stdout; everything here is diagnostics and goes to
 * stderr, so that `bmb_dgemm > results.txt` keeps the two apart. Each
 * kind has its colour on a terminal, none when stderr is not one or when
 * NO_COLOR is set in the environment. */
void bmb_log_warning(const char *message);
void bmb_log_error(const char *message);

/* Under --star (#173), what an instance would say while it sets a point
 * up is held, from bmb_log_hold() on, and once the instances know which
 * of them failed, written by bmb_log_release(1) in the first that did,
 * dropped by bmb_log_release(0) in the others: N copies of one message,
 * or of N, would say no more. What does not fit is dropped. */
void bmb_log_hold(void);
void bmb_log_release(int write);

#endif /* BMB_LOG_H */
