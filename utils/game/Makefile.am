lib_LTLIBRARIES = libgame.la

libgame_la_SOURCES = money.h chicken.h chicken.c money.c

AM_CFLAGS = -I${srcdir}/../../include ${MYSQLCLIENT_CFLAGS} ${ICONV_CFLAGS} \
            -std=c17 -Wall -Werror -Wformat -Wpedantic -Wformat-security -Wconversion \
            -Wdeprecated -finline-functions
LIBS = ${MYSQLCLIENT_LIBS} ${ICONV_LIBS}

libdir = ${prefix}/game/so
