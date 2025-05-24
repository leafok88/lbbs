#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([bbsd],[1.0])
AC_CONFIG_SRCDIR([src/])
# AM_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_GCC_TRADITIONAL
LT_INIT
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([mysqlclient], [mysql_connect])
AC_CHECK_LIB([z], [unzip])

AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h dflcn.h fcntl.h memory.h netdb.h netinet/in.h signal.h stdint.h stdlib.h \
					string.h strings.h sys/epoll.h sys/ioctl.h sys/mman.h sys/param.h sys/socket.h sys/time.h \
					sys/sem.h sys/shm.h sys/ipc.h sys/syscall.h sys/wait.h systemd/sd-daemon.h unistd.h mysql.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_CHECK_HEADERS_ONCE([sys/time.h])

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
