AC_PREREQ([2.72])
AC_INIT([MZ77],[0.1],[gordienkov.zyu@edu.spbstu.ru])
AM_INIT_AUTOMAKE([gnits subdir-objects])
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([GTK4], [gtk4 >= 4.0], [],
		  [AC_MSG_ERROR([GTK4 development files not found. Install libgtk-4-dev])]
		  )

AC_ARG_ENABLE([debug],
	      [AS_HELP_STRING([--enable-debug,], [compile with debugging symbols and profiling])],
	      [if test "x$enable_debug" = xyes; then
	       PROFILE_CFLAGS="-Wall -Wextra -Werror -Wpedantic -Wfatal-errors -pedantic-errors -O0 -g3 -pg -D_FORTIFY_SOURCE=3"
	       fi],
	       [enable_debug=no])
AC_ARG_ENABLE([release],
	      [AS_HELP_STRING([--enable-release], [compile with optimizations for release])],
	      [if test "x$enable_release" = xyes; then
	       PROFILE_CFLAGS="-O3 -DNDEBUG -fhardened"
	       fi],
	       [enable_release=no])
if test "x$enable_debug" = xyes && test "x$enable_release" = xyes; then
	AC_MSG_ERROR([--enable-debug and --enable-release are mututally exclusive])
fi
AC_SUBST([PROFILE_CFLAGS])
AC_SUBST([PROFILE_LDFLAGS])
gl_EARLY
AM_PROG_AR
LT_INIT
gl_INIT
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
 test/Makefile
 lib/Makefile
])
AC_OUTPUT
