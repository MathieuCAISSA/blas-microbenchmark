dnl BMB_CHECK_CFLAGS(FLAG, VARIABLE)
dnl
dnl Appends FLAG to VARIABLE if the C compiler accepts it, and does nothing
dnl otherwise. The test compiles with -Werror because compilers that do not
dnl know a flag typically warn about it rather than fail, which would make
dnl a plain compile test succeed for a flag that has no effect.
AC_DEFUN([BMB_CHECK_CFLAGS], [
  AC_MSG_CHECKING([whether $CC accepts $1])
  bmb_check_cflags_save="$CFLAGS"
  CFLAGS="$CFLAGS $1 -Werror"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
    [AC_MSG_RESULT([yes])
     CFLAGS="$bmb_check_cflags_save"
     $2="[$]{$2:+[$]$2 }$1"],
    [AC_MSG_RESULT([no])
     CFLAGS="$bmb_check_cflags_save"])
])
