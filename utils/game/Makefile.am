lib_LTLIBRARIES = libgame.la

libgame_la_SOURCES = chicken.c money.c

AM_CFLAGS = -Wall -Werror -Wno-unused-parameter -Wformat -Wconversion -Wdeprecated -finline-functions
AM_CPPFLAGS = -I$(top_builddir)/include -I/usr/include/mysql
LIBS = -L/usr/lib/mysql -L/usr/lib64/mysql -lmysqlclient -lz

libdir = ${prefix}/game/so

CLEANFILES = @EXTRA_CLEAN@
