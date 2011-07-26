# autoconf input for ipbt.

AC_INIT([ipbt], [rXXXX], [anakin@pobox.com])
AC_CONFIG_SRCDIR([ipbt.c])

AM_INIT_AUTOMAKE(foreign)

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_CHECK_PROG([HALIBUT],[halibut],[yes],[no])
AM_CONDITIONAL([HAVE_HALIBUT],[test "x$HALIBUT" = "xyes"])

AC_SEARCH_LIBS(log, m)
AC_CHECK_LIB(ncurses, waddch)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
