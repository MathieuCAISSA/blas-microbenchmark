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

/* A batched GEMM this library crashes on (#330): OpenBLAS 0.3.28 to
 * 0.3.34, built with DYNAMIC_ARCH as every distribution builds it, calls
 * its small-matrix kernel through an offset instead of an address in a
 * batch whose M x N x K is at most BMB_BUILD_BATCH_SMALL: the group call
 * from 0.3.28 (OpenMathLib/OpenBLAS#5980), the strided one from 0.3.31
 * (#6086), both fixed for 0.3.35. Given the library's version string
 * (bmb_build_blas_version()) and whether the call is the strided one,
 * which libraries crash, with the upstream issue ("OpenBLAS 0.3.28 to
 * 0.3.34 built with DYNAMIC_ARCH (OpenMathLib/OpenBLAS#5980, fixed in
 * 0.3.35)"), or NULL for a library without the crash. A ".dev" version
 * is taken for the next release, as development is past the last one.
 * Only where OpenBLAS has its small-matrix path: x86_64, arm64, POWER
 * and LoongArch. Exported for its unit test. */
#define BMB_BUILD_BATCH_SMALL (100.0 * 100.0 * 100.0)
const char *bmb_build_batch_crash(const char *blas_version, int strided);

/* The width of the integers the library takes (#172): 64 in an ILP64
 * build (configure --enable-ilp64), 32 otherwise. */
unsigned int bmb_build_int_bits(void);

/* The kernels the library's dispatch chose at run time, as the library
 * names them: "Haswell" (OpenBLAS's core), "zen3" (BLIS's and AOCL-BLAS's
 * configuration), "AVX2" (MKL's code branch). Since SME, AMX and AVX10,
 * the path chosen changes the numbers, and the version string does not
 * tell it (#162). Only where the library says it: Netlib, NVPL, ArmPL
 * and FlexiBLAS do not. Whether it does is fixed when the benchmarks are
 * built, so the outputs ask the preprocessor (BMB_BUILD_HAS_KERNELS,
 * after config.h) rather than test a pointer each coverage job would
 * only find one way. */
#if defined(HAVE_OPENBLAS_GET_CORENAME) || defined(HAVE_BLI_ARCH_QUERY_ID) \
    || defined(HAVE_MKL_CBWR_GET_AUTO_BRANCH)
#define BMB_BUILD_HAS_KERNELS 1
const char *bmb_build_kernels(void);
#endif

/* The compiler the benchmarks were built with, "gcc 13.3.0", and the
 * CFLAGS configure ended with: what the run block of a result names
 * (#161). The BLAS library was built on its own, so these say how the
 * benchmarks and their checks were, not the library. */
const char *bmb_build_compiler(void);
const char *bmb_build_cflags(void);

#endif /* BMB_BUILD_H */
