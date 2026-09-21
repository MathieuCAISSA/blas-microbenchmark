dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([blas-microbenchmark], [0.1.0], [mathieu.caissa@eolen.com])
AC_CONFIG_SRCDIR([src/c/common/bmb_options.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

dnl -----------------------------------------------------------------------
dnl Compiler
dnl -----------------------------------------------------------------------
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR

AS_IF([test "x$GCC" = "xyes"], [
  bmb_gcc_version=`$CC -dumpversion | cut -d. -f1`
  AS_IF([test "$bmb_gcc_version" -lt 12], [
    AC_MSG_WARN([This first implementation targets GCC >= 12; detected GCC $bmb_gcc_version. Proceeding anyway.])
  ])
], [
  AC_MSG_WARN([This first implementation targets GCC >= 12; a non-GCC compiler was detected ($CC).])
])

dnl -----------------------------------------------------------------------
dnl BLAS implementation selection
dnl -----------------------------------------------------------------------
AC_ARG_WITH([blas-libpath],
  [AS_HELP_STRING([--with-blas-libpath=DIR],
    [directory containing the BLAS library to link against (Netlib or OpenBLAS)])],
  [LDFLAGS="-L$withval $LDFLAGS"
   LIBS="-Wl,-rpath,$withval $LIBS"],
  [])

AC_ARG_WITH([cuda-libpath],
  [AS_HELP_STRING([--with-cuda-libpath=DIR],
    [directory containing the CUDA/cuBLAS libraries (reserved: the cuBLAS backend is not implemented in this release)])],
  [AC_MSG_WARN([--with-cuda-libpath was given but the cuBLAS backend is not implemented yet in this release; the value is ignored.])],
  [])

AC_ARG_WITH([rocm-libpath],
  [AS_HELP_STRING([--with-rocm-libpath=DIR],
    [directory containing the ROCm/rocBLAS libraries (reserved: the rocBLAS backend is not implemented in this release)])],
  [AC_MSG_WARN([--with-rocm-libpath was given but the rocBLAS backend is not implemented yet in this release; the value is ignored.])],
  [])

dnl This first implementation targets any CBLAS-compatible library
dnl (Netlib reference CBLAS or OpenBLAS). cuBLAS, rocBLAS, BLIS, NVPL and
dnl ArmPL are reserved for future backends (see README.md).
AC_CHECK_HEADERS([cblas.h], [], [
  AC_MSG_ERROR([cblas.h not found. Install a CBLAS-compatible BLAS (Netlib reference BLAS + CBLAS, or OpenBLAS), or point to it with --with-blas-libpath and CPPFLAGS.])
])

AC_SEARCH_LIBS([cblas_dgemm], [openblas cblas blas], [], [
  AC_MSG_ERROR([No BLAS library exposing the CBLAS interface (cblas_dgemm) was found. Install OpenBLAS or Netlib CBLAS, or pass --with-blas-libpath / LDFLAGS / LIBS.])
])

dnl -----------------------------------------------------------------------
dnl Optional runtime thread-count control, depending on the BLAS backend
dnl -----------------------------------------------------------------------
AC_CHECK_LIB([openblas], [openblas_set_num_threads],
  [AC_DEFINE([HAVE_OPENBLAS_SET_NUM_THREADS], [1],
    [Define if openblas_set_num_threads() is available.])])

AC_OPENMP
AS_IF([test "x$OPENMP_CFLAGS" != "x"], [
  bmb_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $OPENMP_CFLAGS"
  AC_CHECK_FUNCS([omp_set_num_threads])
  CFLAGS="$bmb_save_CFLAGS $OPENMP_CFLAGS"
  LDFLAGS="$LDFLAGS $OPENMP_CFLAGS"
])

AC_CHECK_LIB([m], [sqrt])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  src/c/Makefile
  src/c/common/Makefile
  src/c/level1/Makefile
  src/c/level2/Makefile
  src/c/level3/Makefile
])

AC_OUTPUT

AC_MSG_NOTICE([])
AC_MSG_NOTICE([blas-microbenchmark $VERSION configured:])
AC_MSG_NOTICE([  CC             = $CC])
AC_MSG_NOTICE([  CFLAGS         = $CFLAGS])
AC_MSG_NOTICE([  LDFLAGS        = $LDFLAGS])
AC_MSG_NOTICE([  LIBS           = $LIBS])
AC_MSG_NOTICE([])
