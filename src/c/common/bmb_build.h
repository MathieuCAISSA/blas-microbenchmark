#ifndef BMB_BUILD_H
#define BMB_BUILD_H

/* What this build is, so that a results file can say where its numbers
 * came from. Comparing BLAS libraries is the whole point of the project,
 * and a table that does not name the one it measured is worth very little
 * once it has been sitting in a directory for a month. */

/* The project's own version, e.g. "0.5.0". */
const char *bmb_build_version(void);

/* The backend configure selected, e.g. "openblas". Never "auto": that
 * resolves at configure time to whatever was actually linked. */
const char *bmb_build_backend(void);

/* The BLAS library's own version string, when it exposes one (OpenBLAS
 * and BLIS do), otherwise NULL. This is what tells two files apart when
 * both say "openblas" -- 0.3.20 and 0.3.29 do not perform alike. */
const char *bmb_build_blas_version(void);

#endif /* BMB_BUILD_H */
