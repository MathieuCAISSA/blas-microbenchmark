#ifndef BMB_PRINT_H
#define BMB_PRINT_H

#include <stdio.h>

#include "bmb_options.h"
#include "bmb_result.h"

void bmb_print_format(const char *format, ...);

void bmb_print_txt(FILE *out, const bmb_result_set_t *rs);
void bmb_print_csv(FILE *out, const bmb_result_set_t *rs);
void bmb_print_json(FILE *out, const bmb_result_set_t *rs);

/* Prints the human-readable table to stdout, and additionally writes it
 * to opts->output_file (in opts->output_format) when set. */
void bmb_print_results(const bmb_result_set_t *rs, const bmb_options_t *opts);

#endif /* BMB_PRINT_H */
