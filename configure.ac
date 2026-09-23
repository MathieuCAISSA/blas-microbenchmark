dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([blas-microbenchmark], [0.5.0], [mathieu.caissa@eolen.com])
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

dnl The project holds itself to building warning-free, which is worth
dnl nothing if the warnings are never switched on. Kept out of CFLAGS so
dnl that a user-supplied CFLAGS still comes last and can override them.
AC_ARG_ENABLE([warnings],
  [AS_HELP_STRING([--disable-warnings], [do not add -Wall -Wextra to the build flags])],
  [bmb_warnings="$enableval"], [bmb_warnings=yes])
dnl Not on by default: a compiler newer than this release will eventually
dnl warn about something here, and that must not stop people building it.
dnl CI turns it on, which is what makes "builds warning-free" a fact rather
dnl than an aspiration.
AC_ARG_ENABLE([werror],
  [AS_HELP_STRING([--enable-werror], [treat compiler warnings as errors (used by CI)])],
  [bmb_werror="$enableval"], [bmb_werror=no])
BMB_WARN_CFLAGS=
AS_IF([test "x$bmb_warnings" != xno], [
  BMB_CHECK_CFLAGS([-Wall], [BMB_WARN_CFLAGS])
  BMB_CHECK_CFLAGS([-Wextra], [BMB_WARN_CFLAGS])
  AS_IF([test "x$bmb_werror" = xyes], [BMB_CHECK_CFLAGS([-Werror], [BMB_WARN_CFLAGS])])
])
AC_SUBST([BMB_WARN_CFLAGS])

dnl -----------------------------------------------------------------------
dnl Search-path helpers
dnl
dnl HPC sites normally expose a BLAS install through module files that set
dnl <PKG>_ROOT / <PKG>_INCDIR / <PKG>_LIBDIR instead of installing into
dnl /usr, so honour those too: `module load openblas && ./configure` should
dnl work without hand-written CPPFLAGS/LDFLAGS. Directories are prepended,
dnl so the most specific hint must be added last to win.
dnl -----------------------------------------------------------------------
AC_DEFUN([BMB_ADD_INCDIR], [
  AS_IF([test -n "$1" && test -d "$1"], [
    AC_MSG_NOTICE([using include directory $1])
    CPPFLAGS="-I$1 $CPPFLAGS"
  ])
])

AC_DEFUN([BMB_ADD_LIBDIR], [
  AS_IF([test -n "$1" && test -d "$1"], [
    AC_MSG_NOTICE([using library directory $1])
    LDFLAGS="-L$1 -Wl,-rpath,$1 $LDFLAGS"
  ])
])

dnl BMB_ENV_HINTS(VARPREFIX): pick up $VARPREFIX_ROOT, $VARPREFIX_DIR,
dnl $VARPREFIX_HOME, $VARPREFIX_INC, $VARPREFIX_INCDIR, $VARPREFIX_LIB and
dnl $VARPREFIX_LIBDIR.
dnl
dnl _DIR and _HOME are there because module files are not consistent about
dnl this: plenty of sites name the install prefix one of those instead of
dnl _ROOT, and a benchmark that cannot find the library it was asked for is
dnl no use.
dnl
dnl The prefix-derived paths are guarded on the variable being set: an
dnl unset one would otherwise make "$FOO_ROOT/lib" collapse to "/lib",
dnl which exists and would wrongly end up in -L/-rpath.
AC_DEFUN([BMB_ENV_PREFIX_HINT], [
  AS_IF([test -n "$1"], [
    BMB_ADD_INCDIR([$1/include])
    BMB_ADD_LIBDIR([$1/lib64])
    BMB_ADD_LIBDIR([$1/lib])
  ])
])

AC_DEFUN([BMB_ENV_HINTS], [
  dnl Least specific first: everything here is prepended, so the last one
  dnl applied wins.
  BMB_ENV_PREFIX_HINT([[$]$1[_HOME]])
  BMB_ENV_PREFIX_HINT([[$]$1[_DIR]])
  BMB_ENV_PREFIX_HINT([[$]$1[_ROOT]])
  BMB_ADD_INCDIR([[$]$1[_INC]])
  BMB_ADD_INCDIR([[$]$1[_INCDIR]])
  BMB_ADD_LIBDIR([[$]$1[_LIB]])
  BMB_ADD_LIBDIR([[$]$1[_LIBDIR]])
])

dnl BMB_CHECK_BLAS_VERSION_API(LIBRARY)
dnl
dnl Probes the call a library makes its own version string available
dnl through. A results file saying "openblas" does not distinguish 0.3.20
dnl from 0.3.29, and those do not perform alike; this is what lets it say
dnl which build produced the numbers. Both checks pass an explicit
dnl action-if-found so that AC_CHECK_LIB does not append the library to
dnl LIBS a second time.
AC_DEFUN([BMB_CHECK_BLAS_VERSION_API], [
  AS_CASE([$1],
    [openblas], [
      AC_CHECK_LIB([openblas], [openblas_get_config],
        [AC_DEFINE([HAVE_OPENBLAS_GET_CONFIG], [1],
          [Define if openblas_get_config() is available.])])
    ],
    [blis], [
      AC_CHECK_LIB([blis], [bli_info_get_version_str],
        [AC_DEFINE([HAVE_BLI_INFO_GET_VERSION_STR], [1],
          [Define if bli_info_get_version_str() is available.])])
    ])
])

dnl -----------------------------------------------------------------------
dnl BLAS implementation selection
dnl -----------------------------------------------------------------------
AC_ARG_WITH([blas-incpath],
  [AS_HELP_STRING([--with-blas-incpath=DIR],
    [directory containing the BLAS headers (cblas.h), if not on the default path])],
  [BMB_ADD_INCDIR([$withval])],
  [])

AC_ARG_WITH([blas-libpath],
  [AS_HELP_STRING([--with-blas-libpath=DIR],
    [directory containing the BLAS library to link against])],
  [BMB_ADD_LIBDIR([$withval])],
  [])

AC_ARG_WITH([blas-backend],
  [AS_HELP_STRING([--with-blas-backend=auto|openblas|blis|netlib|nvpl|armpl],
    [BLAS backend to build against (default: auto, i.e. search for any CBLAS-compatible library on the default paths)])],
  [bmb_blas_backend="$withval"],
  [bmb_blas_backend=auto])

AS_CASE(["$bmb_blas_backend"],
  [auto|openblas|blis|netlib|nvpl|armpl], [],
  [AC_MSG_ERROR([--with-blas-backend must be one of: auto, openblas, blis, netlib, nvpl, armpl (got '$bmb_blas_backend')])])

dnl Generic hints, applied whatever the backend. Backend-specific ones are
dnl applied inside each case below, so they take priority over these.
BMB_ENV_HINTS([CBLAS])
BMB_ENV_HINTS([BLAS])

AS_CASE(["$bmb_blas_backend"],

  [openblas], [
    BMB_ENV_HINTS([OPENBLAS])
    AC_CHECK_HEADERS([cblas.h], [], [
      AC_MSG_ERROR([cblas.h not found. Install libopenblas-dev, load your OpenBLAS module (OPENBLAS_ROOT/OPENBLAS_INCDIR are picked up automatically), or pass --with-blas-incpath/CPPFLAGS.])
    ])
    AC_SEARCH_LIBS([cblas_dgemm], [openblas], [], [
      AC_MSG_ERROR([libopenblas not found (cblas_dgemm unresolved). Install libopenblas-dev, load your OpenBLAS module (OPENBLAS_ROOT/OPENBLAS_LIBDIR are picked up automatically), or pass --with-blas-libpath/LDFLAGS.])
    ])
    AC_CHECK_LIB([openblas], [openblas_set_num_threads],
      [AC_DEFINE([HAVE_OPENBLAS_SET_NUM_THREADS], [1],
        [Define if openblas_set_num_threads() is available.])])
    BMB_CHECK_BLAS_VERSION_API([openblas])
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
      dnl -isystem, not -I: BLIS's cblas.h defines static helpers that most
      dnl translation units never call, so -Wunused-function fires twice for
      dnl every benchmark. Those warnings belong to BLIS, and we build with
      dnl -Werror in CI. Hint directories are still added with -I and are
      dnl searched first, so a loaded module keeps winning over this probe.
      CPPFLAGS="-isystem $bmb_blis_dir $CPPFLAGS"
      bmb_blis_libdir=`AS_ECHO(["$bmb_blis_dir"]) | sed 's|/include/|/lib/|'`
      LDFLAGS="-L$bmb_blis_libdir -Wl,-rpath,$bmb_blis_libdir $LDFLAGS"
    ])
    dnl Applied after the distro probe so a loaded module wins over a
    dnl system-wide install.
    BMB_ENV_HINTS([BLIS])
    AC_CHECK_HEADERS([cblas.h], [], [
      AC_MSG_ERROR([BLIS's cblas.h not found. Install libblis-openmp-dev (Debian/Ubuntu), load your BLIS module (BLIS_ROOT/BLIS_INCDIR are picked up automatically), or pass --with-blas-incpath/CPPFLAGS.])
    ])
    AC_SEARCH_LIBS([cblas_dgemm], [blis], [], [
      AC_MSG_ERROR([libblis not found (cblas_dgemm unresolved). Install libblis-openmp-dev, load your BLIS module (BLIS_ROOT/BLIS_LIBDIR are picked up automatically), or pass --with-blas-libpath/LDFLAGS.])
    ])
    AS_IF([test "x$bmb_blis_variant" = xpthread], [LIBS="-lpthread $LIBS"])
    AC_CHECK_LIB([blis], [bli_thread_set_num_threads],
      [AC_DEFINE([HAVE_BLI_THREAD_SET_NUM_THREADS], [1],
        [Define if bli_thread_set_num_threads() is available.])],
      [AC_MSG_WARN([bli_thread_set_num_threads not found in libblis; --thread-count/BLIS_NUM_THREADS will have no effect.])])
    BMB_CHECK_BLAS_VERSION_API([blis])
  ],

  [netlib], [
    dnl Netlib reference BLAS is a Fortran library. Some distributions also
    dnl package a CBLAS layer inside it (Debian/Ubuntu do, declared by
    dnl cblas-netlib.h rather than the usual cblas.h), but plenty of netlib
    dnl builds -- source builds, cluster modules -- have none, so the
    dnl benchmarks get their cblas_* entry points from src/c/netlib instead,
    dnl which forwards to the Fortran symbols. One code path, works either
    dnl way; see AGENTS.md.
    dnl
    dnl The library itself needs care: on Debian/Ubuntu plain -lblas goes
    dnl through update-alternatives and usually lands on OpenBLAS, so look
    dnl for the reference build's own directory first.
    bmb_netlib_libdir=
    for bmb_dir in /usr/lib/*/blas /usr/lib64/blas /usr/lib/blas /usr/local/lib/blas; do
      if test -f "$bmb_dir/libblas.so" || test -f "$bmb_dir/libblas.a"; then
        bmb_netlib_libdir="$bmb_dir"
        break
      fi
    done
    AS_IF([test -n "$bmb_netlib_libdir"], [
      AC_MSG_NOTICE([found netlib reference BLAS in $bmb_netlib_libdir])
      BMB_ADD_LIBDIR([$bmb_netlib_libdir])
    ])
    BMB_ENV_HINTS([NETLIB])
    dnl Only the Fortran ABI is required: the CBLAS side is ours.
    dnl
    dnl A shared libblas.so pulls in its own Fortran runtime, but a static
    dnl libblas.a does not: it leaves xerbla's _gfortran_* symbols for us to
    dnl resolve, which is exactly what cluster modules tend to ship. So try
    dnl the plain link first and retry with -lgfortran before giving up,
    dnl rather than always requiring a Fortran runtime to be installed.
    AC_SEARCH_LIBS([dgemm_], [blas refblas], [], [
      AC_MSG_NOTICE([retrying with the Fortran runtime (-lgfortran)])
      AS_UNSET([ac_cv_search_dgemm_])
      AC_SEARCH_LIBS([dgemm_], [blas refblas],
        [LIBS="$LIBS -lgfortran"],
        [AC_MSG_ERROR([Netlib BLAS not found (dgemm_ unresolved). Install libblas-dev, load your netlib module (NETLIB_ROOT/NETLIB_LIBDIR and BLAS_ROOT/BLAS_LIBDIR are picked up automatically), or pass --with-blas-libpath/LDFLAGS.])],
        [-lgfortran])
    ])
    dnl Reference BLAS has no runtime thread control and no thread-count
    dnl environment variable of its own; make that explicit so bmb_threads.c
    dnl doesn't fall through to the generic OpenMP path, which would have no
    dnl effect on it anyway.
    AC_DEFINE([BMB_NO_THREAD_CONTROL], [1],
      [Define if the selected BLAS backend has no runtime thread-count API.])
  ],

  [nvpl], [
    dnl aarch64-only (NVIDIA Grace / Arm CPUs). libnvpl-blas-dev's
    dnl CBLAS-compatible header lives under nvpl_compat/ (a plain
    dnl /usr/include subdirectory, not on the default search path); the
    dnl libraries themselves install to the standard multiarch libdir.
    for bmb_dir in /usr/include/nvpl_compat /usr/include/*/nvpl_compat; do
      if test -f "$bmb_dir/cblas.h"; then
        CPPFLAGS="-I$bmb_dir $CPPFLAGS"
        break
      fi
    done
    BMB_ENV_HINTS([NVPL])
    AS_IF([test -n "$NVPL_ROOT"], [BMB_ADD_INCDIR([$NVPL_ROOT/include/nvpl_compat])])
    AC_CHECK_HEADERS([cblas.h], [], [
      AC_MSG_ERROR([NVPL's cblas.h not found. Install libnvpl-blas-dev (https://developer.nvidia.com/nvpl), load your NVPL module (NVPL_ROOT/NVPL_INCDIR are picked up automatically), or pass --with-blas-incpath/CPPFLAGS.])
    ])
    AC_SEARCH_LIBS([cblas_dgemm], [nvpl_blas_lp64_gomp nvpl_blas_lp64_seq], [], [
      AC_MSG_ERROR([NVPL BLAS not found (cblas_dgemm unresolved); install libnvpl-blas0/libnvpl-blas-dev or pass --with-blas-libpath/LDFLAGS/LIBS.])
    ])
    dnl Not always auto-pulled in as a DT_NEEDED dependency; safe to add
    dnl even when already linked transitively.
    LIBS="-lnvpl_blas_core $LIBS"
  ],

  [armpl], [
    dnl aarch64-only (Arm servers). The apt package (arm-performance-libraries)
    dnl installs to the fixed /opt/arm/arm-performance-libraries/ prefix;
    dnl older/manual tarball installs use a version-specific
    dnl /opt/arm/armpl_<version>_gcc/ prefix instead. Neither is a
    dnl standard system search path.
    bmb_armpl_dir=
    for bmb_dir in /opt/arm/arm-performance-libraries/include /opt/arm/armpl_*/include /opt/arm/armpl_*_gcc/include; do
      if test -f "$bmb_dir/armpl.h"; then
        bmb_armpl_dir="$bmb_dir"
        break
      fi
    done
    AS_IF([test -n "$bmb_armpl_dir"], [
      AC_MSG_NOTICE([found ArmPL headers in $bmb_armpl_dir])
      CPPFLAGS="-I$bmb_armpl_dir $CPPFLAGS"
      bmb_armpl_libdir=`AS_ECHO(["$bmb_armpl_dir"]) | sed 's|/include$|/lib|'`
      LDFLAGS="-L$bmb_armpl_libdir -Wl,-rpath,$bmb_armpl_libdir $LDFLAGS"
    ])
    BMB_ENV_HINTS([ARMPL])
    AS_IF([test -n "$ARMPL_DIR"], [
      BMB_ADD_INCDIR([$ARMPL_DIR/include])
      BMB_ADD_LIBDIR([$ARMPL_DIR/lib])
    ])
    AC_CHECK_HEADERS([cblas.h], [], [
      AC_MSG_ERROR([ArmPL's cblas.h not found. Install arm-performance-libraries (https://developer.arm.com/tools-and-software/arm-performance-libraries), load your ArmPL module (ARMPL_ROOT/ARMPL_DIR/ARMPL_INCDIR are picked up automatically), or pass --with-blas-incpath/CPPFLAGS.])
    ])
    AC_SEARCH_LIBS([cblas_dgemm], [armpl armpl_lp64 armpl_lp64_mp], [], [
      AC_MSG_ERROR([ArmPL not found (cblas_dgemm unresolved); install arm-performance-libraries or pass --with-blas-libpath/LDFLAGS/LIBS.])
    ])
  ],

  [dnl auto: search the default paths, plus OpenBLAS's module variables
   dnl (OpenBLAS is what 'auto' prefers; use --with-blas-backend to pick
   dnl another one and get its variables honoured instead).
    BMB_ENV_HINTS([OPENBLAS])
    AC_CHECK_HEADERS([cblas.h], [], [
      AC_MSG_ERROR([cblas.h not found. Install a CBLAS-compatible BLAS (OpenBLAS, BLIS, ...), load its module (OPENBLAS_ROOT/OPENBLAS_INCDIR, BLAS_ROOT/BLAS_INCDIR are picked up automatically), pass --with-blas-backend to pick one explicitly, or point to it with --with-blas-incpath/CPPFLAGS.])
    ])
    AC_SEARCH_LIBS([cblas_dgemm], [openblas blis cblas blas], [], [
      AC_MSG_ERROR([No BLAS library exposing the CBLAS interface (cblas_dgemm) was found. Load your BLAS module (OPENBLAS_ROOT/OPENBLAS_LIBDIR, BLAS_ROOT/BLAS_LIBDIR are picked up automatically), or pass --with-blas-backend/--with-blas-libpath/LDFLAGS/LIBS.])
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
        BMB_CHECK_BLAS_VERSION_API([openblas])
        bmb_blas_backend_name=openblas
      ],
      [*-lblis*], [
        AC_CHECK_LIB([blis], [bli_thread_set_num_threads],
          [AC_DEFINE([HAVE_BLI_THREAD_SET_NUM_THREADS], [1],
            [Define if bli_thread_set_num_threads() is available.])])
        BMB_CHECK_BLAS_VERSION_API([blis])
        bmb_blas_backend_name=blis
      ]
    )
  ]
)

dnl What the results files will say they were produced with. 'auto' is not
dnl an answer anyone can act on three months later, so it resolves to
dnl whatever AC_SEARCH_LIBS actually linked.
AS_IF([test "x$bmb_blas_backend" != xauto], [bmb_blas_backend_name="$bmb_blas_backend"])
AS_IF([test -z "$bmb_blas_backend_name"], [bmb_blas_backend_name=unknown])
AC_DEFINE_UNQUOTED([BMB_BLAS_BACKEND], ["$bmb_blas_backend_name"],
  [Name of the BLAS backend this build was configured against.])

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

dnl The netlib backend supplies its own cblas_* layer from src/c/netlib;
dnl that directory must be on the include path ahead of anything else, so
dnl the benchmarks' #include <cblas.h> resolves to it.
AM_CONDITIONAL([BLAS_BACKEND_NETLIB], [test "x$bmb_blas_backend" = xnetlib])
AS_IF([test "x$bmb_blas_backend" = xnetlib],
  [BMB_NETLIB_CPPFLAGS='-I$(top_srcdir)/src/c/netlib'],
  [BMB_NETLIB_CPPFLAGS=])
AC_SUBST([BMB_NETLIB_CPPFLAGS])

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
AC_MSG_NOTICE([])
