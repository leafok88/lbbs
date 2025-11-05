lib_LTLIBRARIES = libgame.la

libgame_la_SOURCES = money.h chicken.c money.c

AM_CFLAGS = -I${srcdir}/../../include ${MYSQLCLIENT_CFLAGS} -std=c17 -Wall -Werror \
            -Wno-unused-parameter -Wformat -Wpedantic -Wformat-security -Wconversion \
            -Wdeprecated -finline-functions
LIBS = ${MYSQLCLIENT_LIBS} -lz

libdir = ${prefix}/game/so
