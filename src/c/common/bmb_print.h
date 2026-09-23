#ifndef BMB_PRINT_H
#define BMB_PRINT_H

#include <stdio.h>

#include "bmb_options.h"
#include "bmb_result.h"

/* The human-readable table is printed as it is measured rather than at the
 * end: a sweep over large sizes can run for many minutes, and showing
 * nothing until it finishes makes it impossible to tell progress from a
 * hang -- and loses everything if it is interrupted. */
void bmb_print_txt_begin(FILE *out, const bmb_result_set_t *rs);
void bmb_print_txt_row(FILE *out, const bmb_result_set_t *rs,
                        const bmb_result_row_t *row);

void bmb_print_csv(FILE *out, const bmb_result_set_t *rs);
void bmb_print_json(FILE *out, const bmb_result_set_t *rs);

/* Writes the collected results to opts->output_file in opts->output_format,
 * or does nothing when no output file was requested. Returns -1 if the file
 * could not be written in full (an error message has been printed) -- a
 * results file truncated by a full disk must not look like a successful
 * run. */
int bmb_print_save(const bmb_result_set_t *rs, const bmb_options_t *opts);

/* Returns -1 if anything written to stream failed, which for stdout is
 * otherwise invisible: a closed pipe or a full filesystem silently drops
 * the table. */
int bmb_print_check_stream(FILE *stream, const char *what);

#endif /* BMB_PRINT_H */
