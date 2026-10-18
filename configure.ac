AC_PREREQ([2.72])
AC_INIT([MZ77],[0.1],[gordienkov.zyu@edu.spbstu.ru])
AM_INIT_AUTOMAKE([gnits subdir-objects])
dnl Remember whether the user set CFLAGS before AC_PROG_CC adds -g -O2.
mz77_user_cflags=${CFLAGS+set}
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([GTK4], [gtk4 >= 4.0], [],
		  [AC_MSG_ERROR([GTK4 development files not found. Install libgtk-4-dev])]
		  )

AC_ARG_ENABLE([debug],
	      [AS_HELP_STRING([--enable-debug,], [compile with debugging symbols])],
	      [if test "x$enable_debug" = xyes; then
	       PROFILE_CFLAGS="-Wall -Wextra -Werror -Wpedantic -Wfatal-errors -pedantic-errors -O0 -g3 -D_FORTIFY_SOURCE=3"
	       fi],
	       [enable_debug=no])
AC_ARG_ENABLE([release],
	      [AS_HELP_STRING([--enable-release], [compile with optimizations for release])],
	      [if test "x$enable_release" = xyes; then
	       PROFILE_CFLAGS="-O3 -DNDEBUG -fhardened"
	       fi],
	       [enable_release=no])
AC_ARG_ENABLE([profile],
	      [AS_HELP_STRING([--enable-profile], [compile optimized code instrumented for gprof])],
	      [if test "x$enable_profile" = xyes; then
	       PROFILE_CFLAGS="-O2 -g -pg -fno-inline -fno-omit-frame-pointer"
	       PROFILE_LDFLAGS="-pg"
	       fi],
	       [enable_profile=no])
profile_count=0
for profile_choice in "$enable_debug" "$enable_release" "$enable_profile"; do
	test "x$profile_choice" = xyes && profile_count=`expr $profile_count + 1`
done
if test $profile_count -gt 1; then
	AC_MSG_ERROR([--enable-debug, --enable-release and --enable-profile are mutually exclusive])
fi
dnl CFLAGS comes after PROFILE_CFLAGS on the command line, so the default
dnl -g -O2 would override the profile's -O level.  Drop the default, but
dnl keep CFLAGS the user gave.
if test -n "$PROFILE_CFLAGS" && test "x$mz77_user_cflags" != xset; then
	CFLAGS=
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
