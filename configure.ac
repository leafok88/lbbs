#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([lbbs],[1.5.6])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES(yes)

AC_USE_SYSTEM_EXTENSIONS
# Checks for programs.
LT_INIT
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_ARG_WITH([sysv],
            [AS_HELP_STRING([--with-sysv], [Use System V specific APIs])],
            [AS_IF([test "x$with_sysv" = xyes],
                   [AC_DEFINE([HAVE_SYSTEM_V], [1], [Define if System V is available])])],
            [with_sysv="yes"])
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [Enable debug log])],
              [AS_IF([test "x$enable_debug" = xyes],
                     [AC_DEFINE([_DEBUG], [1], [Define if debug log is enabled])])])

AC_ARG_WITH([iconv],
            [AS_HELP_STRING([--with-iconv], [Use iconv library package config])],
            [],
            [with_iconv="no"])
AC_ARG_WITH([mariadb],
            [AS_HELP_STRING([--with-mariadb], [Use MariaDB library instead of MySQL client library])],
            [AS_IF([test "x$with_mariadb" = xyes],
                   [AC_DEFINE([HAVE_MARIADB_CLIENT], [1], [Define if MariaDB client is used])])],
            [with_mariadb="no"])
AC_ARG_ENABLE([systemd],
              [AS_HELP_STRING([--enable-systemd], [Enable systemd daemon])],
              [],
              [enable_systemd="no"])
AC_ARG_WITH([epoll],
            [AS_HELP_STRING([--with-epoll], [Use epoll instead of poll])],
            [],
            [with_epoll="yes"])

AC_SUBST([PKG_CONFIG_PATH])
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([LIBSSH], [libssh], [], [AC_MSG_ERROR([libssh is required.])])
PKG_CHECK_MODULES([LIBPCRE2], [libpcre2-8], [], [AC_MSG_ERROR([libpcre2-8 is required.])])

AS_IF([test "x$with_iconv" = xyes],
      [PKG_CHECK_MODULES([ICONV], [iconv], [], [AC_MSG_ERROR([iconv is required.])])])

AS_IF([test "x$with_mariadb" = xyes],
      [PKG_CHECK_MODULES([MYSQLCLIENT], [libmariadb], [], [AC_MSG_ERROR([libmariadb is required.])])],
      [PKG_CHECK_MODULES([MYSQLCLIENT], [mysqlclient], [], [AC_MSG_ERROR([mysqlclient is required.])])])

AS_IF([test "x$enable_systemd" = xyes],
      [PKG_CHECK_MODULES([SYSTEMD], [libsystemd], [], [AC_MSG_ERROR([libsystemd is required.])])])

AM_CONDITIONAL([LINK_USE_SO], [test "x$with_sysv" = xyes])
AS_IF([test "x$with_sysv" = xyes],
      [AC_DEFINE([LOAD_SO], [1], [Define if shared object is usable])])

CPPFLAGS="$CPPFLAGS $LIBSSH_CFLAGS $MYSQLCLIENT_CFLAGS $SYSTEMD_CFLAGS $LIBPCRE2_CFLAGS $ICONV_CFLAGS"

# Checks for libraries.
AC_HEADER_STAT
AC_PROG_EGREP

AC_HEADER_RESOLV
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h iconv.h libssh/libssh.h mysql.h netdb.h netinet/in.h pcre2.h pty.h \
                  semaphore.h signal.h stdint.h stdlib.h string.h sys/ioctl.h sys/mman.h sys/param.h sys/socket.h \
                  sys/types.h sys/sem.h sys/stat.h sys/wait.h time.h unistd.h utmp.h wchar.h],
                  [],
                  [AC_MSG_ERROR([Missing required header file])],
                  [#define PCRE2_CODE_UNIT_WIDTH 8])

AS_IF([test "x$enable_systemd" = xyes],
      [AC_CHECK_HEADERS([systemd/sd-daemon.h], [], [AC_MSG_ERROR([systemd/sd-daemon.h not found.])])])

AS_IF([test "x$with_epoll" = xyes],
      [AC_CHECK_HEADERS([sys/epoll.h], [], [AC_MSG_ERROR([sys/epoll.h not found.])])],
      [AC_CHECK_HEADERS([poll.h], [], [AC_MSG_ERROR([poll.h not found.])])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_MMAP
AC_FUNC_MKTIME

AC_DEFINE_UNQUOTED([RETSIGTYPE],
                   [$ac_cv_type_signal],
                   [Define as the return type of signal handlers (`int' or `void').])

AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([dup2 gethostbyname inet_ntoa memset mkdir rmdir regcomp poll socket strstr])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 utils/Makefile
                 utils/game/Makefile
                 conf/lbbs.service
                 conf/lbbs.logrotate])

AC_OUTPUT
