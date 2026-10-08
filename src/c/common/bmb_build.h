#ifndef BMB_BUILD_H
#define BMB_BUILD_H

#include <stddef.h>

/* What this build is, so that a results file can say where its numbers
 * came from. Comparing BLAS libraries is the whole point of the project,
 * and a table that does not name the one it measured is worth very little
 * once it has been sitting in a directory for a month. */

/* The project's own version, e.g. "0.5.0". */
const char *bmb_build_version(void);

/* The backend configure selected, e.g. "openblas". Never "auto": that
 * resolves at configure time to whatever was actually linked. With
 * FlexiBLAS, the library it loaded at run time: "flexiblas/blis-openmp". */
const char *bmb_build_backend(void);

/* "flexiblas/<library>", the backend a FlexiBLAS build names: the library
 * FlexiBLAS loaded, as FlexiBLAS names it, lower-cased and kept to
 * [a-z0-9._+-] (anything else becomes "-"). FlexiBLAS takes those names
 * from configuration files, the user's included, and the backend goes into
 * the series key and the output. Exported for its unit test. */
void bmb_build_backend_name(const char *library, char *buf, size_t size);

/* The BLAS library's own version string, when it exposes one (OpenBLAS
 * and BLIS do), otherwise NULL. This is what tells two files apart when
 * both say "openblas" -- 0.3.20 and 0.3.29 do not perform alike. */
const char *bmb_build_blas_version(void);

#endif /* BMB_BUILD_H */
