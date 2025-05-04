lib_LTLIBRARIES = libgame.la

libgame_la_SOURCES = chicken.c

AM_CPPFLAGS = -I$(top_builddir)/include -I/usr/include/mysql
LIBS = -L$(top_builddir)/lib -L/usr/lib/mysql -L/usr/lib64/mysql -lmysqlclient -lz
AM_CFLAGS=-I$(top_builddir)/src

libdir=${prefix}/game/so

CLEANFILES=@EXTRA_CLEAN@
