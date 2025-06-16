lib_LTLIBRARIES = libgame.la

libgame_la_SOURCES = chicken.c money.c

AM_CFLAGS = -std=c17 -Wall -Werror -Wno-unused-parameter -Wformat -Wconversion -Wdeprecated -finline-functions
AM_CPPFLAGS = -I$(top_builddir)/include
LIBS = -L/usr/lib64/mysql/ -lmysqlclient -lz

libdir = ${prefix}/game/so

CLEANFILES = @EXTRA_CLEAN@
