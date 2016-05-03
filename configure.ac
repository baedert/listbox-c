AC_INIT([listbox-test], [1.0])

AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_CONFIG_MACRO_DIRS([m4])
LT_INIT

PKG_CHECK_MODULES(LISTBOX, gtk+-3.0)

CFLAGS+=" -g -O0 -fno-omit-frame-pointer -Wextra"

AC_CONFIG_FILES([
  Makefile
])

AC_OUTPUT
