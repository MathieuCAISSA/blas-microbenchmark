#ifndef BMB_LOG_H
#define BMB_LOG_H

/* Prints message to stdout, wrapped in the given ANSI color escape code
 * (e.g. "\033[33m"), or with no color if color is NULL. */
void bmb_log_format(const char *message, const char *color);

void bmb_log_info(const char *message);
void bmb_log_debug(const char *message);
void bmb_log_warning(const char *message);
void bmb_log_error(const char *message);

#endif /* BMB_LOG_H */
