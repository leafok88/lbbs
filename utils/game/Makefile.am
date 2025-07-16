lib_LTLIBRARIES = libgame.la

libgame_la_SOURCES = money.h chicken.c money.c

AM_CFLAGS = -I${srcdir}/../../include -std=c17 -Wall -Werror -Wno-unused-parameter -Wformat -Wformat-security -Wconversion -Wdeprecated -finline-functions
LIBS = -L/usr/lib64/mysql/ -lmysqlclient -lz

libdir = ${prefix}/game/so
