#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([bbsd],[1.0])
AC_CONFIG_SRCDIR([src/])
# AM_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_GCC_TRADITIONAL
LT_INIT
AC_PROG_RANLIB
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([mysqlclient], [mysql_connect])
AC_CHECK_LIB([z], [unzip])

# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP

AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h dflcn.h fcntl.h memory.h netdb.h netinet/in.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_CHECK_HEADERS_ONCE([sys/time.h])
# Obsolete code to be removed.
if test $ac_cv_header_sys_time_h = yes; then
  AC_DEFINE([TIME_WITH_SYS_TIME],[1],[Define to 1 if you can safely include both <sys/time.h>
	     and <time.h>.  This macro is obsolete.])
fi
# End of obsolete code.

AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES

AC_DEFINE_UNQUOTED([RETSIGTYPE],[$ac_cv_type_signal],[Define as the return type of signal handlers
		    (`int' or `void').])

AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero dup2 gethostbyname inet_ntoa memset mkdir rmdir regcomp select socket strstr])

AC_CONFIG_FILES([Makefile
		src/Makefile
		utils/Makefile
                utils/game/Makefile])
AC_OUTPUT
