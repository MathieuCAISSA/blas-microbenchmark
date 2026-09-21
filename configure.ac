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
    [directory containing the BLAS library to link against])],
  [LDFLAGS="-L$withval $LDFLAGS"
   LIBS="-Wl,-rpath,$withval $LIBS"],
  [])

AC_ARG_WITH([blas-backend],
  [AS_HELP_STRING([--with-blas-backend=auto|openblas|blis|nvpl|armpl],
    [BLAS backend to build against (default: auto, i.e. search for any CBLAS-compatible library on the default paths)])],
  [bmb_blas_backend="$withval"],
  [bmb_blas_backend=auto])

AS_CASE(["$bmb_blas_backend"],
  [auto|openblas|blis|nvpl|armpl], [],
  [AC_MSG_ERROR([--with-blas-backend must be one of: auto, openblas, blis, nvpl, armpl (got '$bmb_blas_backend')])])

dnl netlib reference BLAS ships no CBLAS C wrapper on Debian/Ubuntu (only
dnl the raw Fortran ABI), so it is intentionally not one of the backends
dnl selectable here; see AGENTS.md.
AS_CASE(["$bmb_blas_backend"],

  [openblas], [
    AC_CHECK_HEADERS([cblas.h], [], [
      AC_MSG_ERROR([cblas.h not found; install libopenblas-dev or pass --with-blas-libpath/CPPFLAGS.])
    ])
    AC_SEARCH_LIBS([cblas_dgemm], [openblas], [], [
      AC_MSG_ERROR([libopenblas not found (cblas_dgemm unresolved); install libopenblas-dev or pass --with-blas-libpath/LDFLAGS.])
    ])
    AC_CHECK_LIB([openblas], [openblas_set_num_threads],
      [AC_DEFINE([HAVE_OPENBLAS_SET_NUM_THREADS], [1],
        [Define if openblas_set_num_threads() is available.])])
  ],

  [blis], [
    dnl BLIS installs its cblas.h under a variant-specific, non-default
    dnl directory (e.g. /usr/include/x86_64-linux-gnu/blis-openmp on
    dnl Debian/Ubuntu's libblis-openmp-dev), so probe common locations
    dnl before falling back to the default search path.
    bmb_blis_dir=
    bmb_blis_variant=
    for bmb_variant in openmp pthread serial; do
      for bmb_dir in /usr/include/*/blis-$bmb_variant /usr/local/include/blis-$bmb_variant; do
        if test -f "$bmb_dir/cblas.h"; then
          bmb_blis_dir="$bmb_dir"
          bmb_blis_variant="$bmb_variant"
          break 2
        fi
      done
    done
    AS_IF([test -n "$bmb_blis_dir"], [
      AC_MSG_NOTICE([found BLIS ($bmb_blis_variant) headers in $bmb_blis_dir])
      CPPFLAGS="-I$bmb_blis_dir $CPPFLAGS"
      bmb_blis_libdir=`AS_ECHO(["$bmb_blis_dir"]) | sed 's|/include/|/lib/|'`
      LDFLAGS="-L$bmb_blis_libdir -Wl,-rpath,$bmb_blis_libdir $LDFLAGS"
    ])
    AC_CHECK_HEADERS([cblas.h], [], [
      AC_MSG_ERROR([BLIS's cblas.h not found. Install libblis-openmp-dev (Debian/Ubuntu) or point to it with --with-blas-libpath/CPPFLAGS.])
    ])
    AC_SEARCH_LIBS([cblas_dgemm], [blis], [], [
      AC_MSG_ERROR([libblis not found (cblas_dgemm unresolved); install libblis-openmp-dev or pass --with-blas-libpath/LDFLAGS.])
    ])
    AS_IF([test "x$bmb_blis_variant" = xpthread], [LIBS="-lpthread $LIBS"])
    AC_CHECK_LIB([blis], [bli_thread_set_num_threads],
      [AC_DEFINE([HAVE_BLI_THREAD_SET_NUM_THREADS], [1],
        [Define if bli_thread_set_num_threads() is available.])],
      [AC_MSG_WARN([bli_thread_set_num_threads not found in libblis; --thread-count/BLIS_NUM_THREADS will have no effect.])])
  ],

  [nvpl], [
    AC_MSG_WARN([--with-blas-backend=nvpl is best-effort: NVPL targets aarch64 (NVIDIA Grace) and could not be built or run in the environment this project was developed in. Wiring follows NVPL's published CBLAS-compatible interface but is otherwise unverified.])
    AC_CHECK_HEADERS([nvpl_blas_cblas.h])
    AC_CHECK_HEADERS([cblas.h])
    AC_SEARCH_LIBS([cblas_dgemm], [nvpl_blas_lp64_gomp nvpl_blas_lp64_seq], [], [
      AC_MSG_ERROR([NVPL BLAS not found (cblas_dgemm unresolved); pass --with-blas-libpath/CPPFLAGS/LDFLAGS/LIBS pointing to your NVPL install.])
    ])
  ],

  [armpl], [
    AC_MSG_WARN([--with-blas-backend=armpl is best-effort: ArmPL requires an Arm-provided install and could not be built or run in the environment this project was developed in. Wiring follows ArmPL's published CBLAS-compatible interface but is otherwise unverified.])
    AC_CHECK_HEADERS([armpl.h])
    AC_CHECK_HEADERS([cblas.h])
    AC_SEARCH_LIBS([cblas_dgemm], [armpl armpl_lp64 armpl_lp64_mp], [], [
      AC_MSG_ERROR([ArmPL not found (cblas_dgemm unresolved); pass --with-blas-libpath/CPPFLAGS/LDFLAGS/LIBS pointing to your ArmPL install.])
    ])
  ],

  [dnl auto: current/original behavior, unchanged.
    AC_CHECK_HEADERS([cblas.h], [], [
      AC_MSG_ERROR([cblas.h not found. Install a CBLAS-compatible BLAS (OpenBLAS, BLIS, ...), pass --with-blas-backend to pick one explicitly, or point to it with --with-blas-libpath/CPPFLAGS.])
    ])
    AC_SEARCH_LIBS([cblas_dgemm], [openblas blis cblas blas], [], [
      AC_MSG_ERROR([No BLAS library exposing the CBLAS interface (cblas_dgemm) was found. Pass --with-blas-backend, --with-blas-libpath, or LDFLAGS/LIBS.])
    ])
    dnl 'auto' doesn't know ahead of time which backend it will find, so
    dnl only probe the thread-control API of whichever library
    dnl AC_SEARCH_LIBS actually picked for cblas_dgemm above (checking
    dnl both unconditionally would risk AC_DEFINE-ing a thread API from a
    dnl library that never ends up in LIBS, e.g. when both OpenBLAS and
    dnl BLIS happen to be installed but only one is actually linked).
    AS_CASE(["$ac_cv_search_cblas_dgemm"],
      [*-lopenblas*], [
        AC_CHECK_LIB([openblas], [openblas_set_num_threads],
          [AC_DEFINE([HAVE_OPENBLAS_SET_NUM_THREADS], [1],
            [Define if openblas_set_num_threads() is available.])])
      ],
      [*-lblis*], [
        AC_CHECK_LIB([blis], [bli_thread_set_num_threads],
          [AC_DEFINE([HAVE_BLI_THREAD_SET_NUM_THREADS], [1],
            [Define if bli_thread_set_num_threads() is available.])])
      ]
    )
  ]
)

dnl -----------------------------------------------------------------------
dnl Optional runtime thread-count control via OpenMP (OpenBLAS-OpenMP,
dnl NVPL, ArmPL all use omp_set_num_threads()/OMP_NUM_THREADS).
dnl -----------------------------------------------------------------------
AC_OPENMP
AS_IF([test "x$OPENMP_CFLAGS" != "x"], [
  bmb_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $OPENMP_CFLAGS"
  AC_CHECK_FUNCS([omp_set_num_threads])
  CFLAGS="$bmb_save_CFLAGS $OPENMP_CFLAGS"
  LDFLAGS="$LDFLAGS $OPENMP_CFLAGS"
])

AC_CHECK_LIB([m], [sqrt])

dnl -----------------------------------------------------------------------
dnl GPU backends (cuBLAS / rocBLAS) - optional, at most one at a time
dnl -----------------------------------------------------------------------
AC_ARG_WITH([gpu-backend],
  [AS_HELP_STRING([--with-gpu-backend=none|cuda|rocm],
    [additionally build the *_gpu benchmarks against this GPU BLAS backend (default: none). Requires the matching toolkit: CUDA+cuBLAS (nvidia-cuda-toolkit), or ROCm+rocBLAS (librocblas-dev, libamdhip64-dev).])],
  [bmb_gpu_backend="$withval"],
  [bmb_gpu_backend=none])

AS_CASE(["$bmb_gpu_backend"],
  [none|cuda|rocm], [],
  [AC_MSG_ERROR([--with-gpu-backend must be one of: none, cuda, rocm (got '$bmb_gpu_backend')])])

AC_ARG_WITH([cuda-libpath],
  [AS_HELP_STRING([--with-cuda-libpath=DIR],
    [directory containing libcublas/libcudart, if not already on the default library path])],
  [LDFLAGS="-L$withval -Wl,-rpath,$withval $LDFLAGS"],
  [])

AC_ARG_WITH([rocm-libpath],
  [AS_HELP_STRING([--with-rocm-libpath=DIR],
    [directory containing librocblas/libamdhip64, if not already on the default library path])],
  [LDFLAGS="-L$withval -Wl,-rpath,$withval $LDFLAGS"],
  [])

bmb_have_gpu_cuda=no
bmb_have_gpu_rocm=no

AS_CASE(["$bmb_gpu_backend"],

  [cuda], [
    AC_CHECK_HEADERS([cublas_v2.h], [], [
      AC_MSG_ERROR([cublas_v2.h not found; install nvidia-cuda-toolkit or pass --with-cuda-libpath/CPPFLAGS.])
    ])
    AC_CHECK_LIB([cudart], [cudaMalloc], [], [
      AC_MSG_ERROR([libcudart not found; install nvidia-cuda-toolkit or pass --with-cuda-libpath.])
    ])
    AC_CHECK_LIB([cublas], [cublasCreate_v2], [], [
      AC_MSG_ERROR([libcublas not found; install nvidia-cuda-toolkit or pass --with-cuda-libpath.])
    ])
    bmb_have_gpu_cuda=yes
  ],

  [rocm], [
    dnl HIP's headers refuse to compile unless exactly one of these is
    dnl defined; we always target the AMD/ROCm platform, never HIP-on-CUDA.
    CPPFLAGS="-D__HIP_PLATFORM_AMD__ $CPPFLAGS"
    AC_CHECK_HEADERS([rocblas/rocblas.h], [], [
      AC_MSG_ERROR([rocblas/rocblas.h not found; install librocblas-dev or pass --with-rocm-libpath/CPPFLAGS.])
    ])
    AC_CHECK_HEADERS([hip/hip_runtime.h], [], [
      AC_MSG_ERROR([hip/hip_runtime.h not found; install libamdhip64-dev or pass --with-rocm-libpath/CPPFLAGS.])
    ])
    AC_CHECK_LIB([amdhip64], [hipMalloc], [], [
      AC_MSG_ERROR([libamdhip64 not found; install libamdhip64-dev or pass --with-rocm-libpath.])
    ])
    AC_CHECK_LIB([rocblas], [rocblas_create_handle], [], [
      AC_MSG_ERROR([librocblas not found; install librocblas-dev or pass --with-rocm-libpath.])
    ])
    bmb_have_gpu_rocm=yes
  ]
)

AM_CONDITIONAL([HAVE_GPU_CUDA], [test "x$bmb_have_gpu_cuda" = xyes])
AM_CONDITIONAL([HAVE_GPU_ROCM], [test "x$bmb_have_gpu_rocm" = xyes])
AM_CONDITIONAL([HAVE_ANY_GPU], [test "x$bmb_have_gpu_cuda" = xyes -o "x$bmb_have_gpu_rocm" = xyes])

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
AC_MSG_NOTICE([  BLAS backend   = $bmb_blas_backend])
AC_MSG_NOTICE([  GPU backend    = $bmb_gpu_backend])
AC_MSG_NOTICE([])
