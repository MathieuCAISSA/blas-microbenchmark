#ifndef BMB_MKL_CBLAS_H
#define BMB_MKL_CBLAS_H

/* MKL declares its CBLAS in mkl_cblas.h, not cblas.h, which every
 * benchmark includes. This directory is on the include path only when
 * --with-blas-backend=mkl is selected, as src/c/netlib is for Netlib, so
 * it never shadows the cblas.h of another backend. */
#include <mkl_cblas.h>

#endif /* BMB_MKL_CBLAS_H */
